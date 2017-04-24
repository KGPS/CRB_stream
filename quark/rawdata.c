/*
 * Copyright (c) 2015, Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include "util/misc.h"
#include "os/os.h"
#include "infra/log.h"

#include "cfw/cfw.h"

#include "cir_storage.h"
#include "services/circular_storage_service/circular_storage_service.h"
#include "drivers/data_type.h"
#include "project_mapping.h"
#include "rawdata.h"
#include "infra/time.h"

/* BLE */
#include "lib/ble/ble_app.h"
#include "iasp.h"

/* IQs */
#include "iq/raw_sensor_streaming.h"
#include "itm/itm.h"

/* Maximum expected latency in ms */
#define MAXIMUM_LATENCY  100

/* Non official channel */
#define IASP_RAWDATA_CHANNEL    0x1C

static bool con_opened = false;
static uint8_t nb_pending_raw_data = 0;
static uint8_t nb_subscribe_expected = 0;
static uint8_t nb_subscribe_rsp = 0;
static bool buffer_empty = false;
static bool use_stream = false;

/* Client */
static cfw_client_t *client = NULL;

/* Sensors client */
static cfw_service_conn_t *sensor_service_conn = NULL;

/* Sensor handles */
static sensor_service_t handles[ON_BOARD_SENSOR_TYPE_END + 1] = { NULL };

static cfw_service_conn_t *circular_storage_service_conn = NULL;
static cir_storage_t *storage = NULL;
static bool session_running = false;
static struct sensor_subscribe_parameters {
	uint32_t sensor_mask;
	uint32_t frequency;
} sensor_parameter;

static uint32_t sampling_interval = 0;

/* This structure represents the data stored in the circular storage */
struct stored_data {
	uint32_t timestamp;
	/* Data collected between timestamp and timestamp + interval/2 */
	uint8_t data[RAW_STORAGE_ELT_SIZE - sizeof(uint32_t) - sizeof(uint8_t)];
	/* Actual size of the data = timestamp + valid portion of data array */
	uint8_t datasize;
};
STATIC_ASSERT(sizeof(struct stored_data) == RAW_STORAGE_ELT_SIZE);

static struct stored_data tmp_data;
static uint32_t data_index = 0;

/* 1 byte for type and 1 byte for length */
#define DATA_HEADER_SIZE    (2 * sizeof(uint8_t))

/* Define the maximum number of pending IASP messages */
#define RAWDATA_IASP_MAX_MSGS 3

void iasp_rawdata_channel_handler(const struct iasp_event *p_iasp_evt);

struct iasp_channel raw_data_iasp = {
	.id = IASP_RAWDATA_CHANNEL,
	.handler = iasp_rawdata_channel_handler,
	.next = NULL,
};

static void check_end_of_session(void)
{
	/* During streaming, If no more BLE ack pending and
	 * no more data to pull  and
	 * session is over => trig response */
	if (use_stream && !nb_pending_raw_data && buffer_empty &&
	    !session_running) {
		/* Restore the default BLE connection parameters and ack the stop
		 * request */
		pr_info(LOG_MODULE_MAIN, "Raw data session is over");
		raw_sensor_streaming_iq_send_itm_response(
			TOPIC_STATUS_OK);
		ble_app_restore_default_conn();
	}
}

void iasp_rawdata_channel_handler(const struct iasp_event *p_iasp_evt)
{
	switch (p_iasp_evt->event) {
	case IASP_OPEN:
		pr_debug(LOG_MODULE_MAIN, "CONN IASP OPEN...");
		con_opened = true;
		break;

	case IASP_CLOSE:
		pr_debug(LOG_MODULE_MAIN, "CONN IASP CLOSE...");
		con_opened = false;
		/* Restore the default BLE connection parameters if session running */
		if (session_running && use_stream) {
			ble_app_restore_default_conn();
			/* Stop raw data collection when BLE connection is closed */
			rawdata_end();
		}
		break;

	case IASP_RX_COMPLETE:
		break;

	case IASP_TX_COMPLETE:
		if ((nb_pending_raw_data >= RAWDATA_IASP_MAX_MSGS) &&
		    con_opened) {
			/* resume peeking the data */
			circular_storage_service_peek(
				circular_storage_service_conn,
				storage,
				NULL);
		}
		/* Decrease the number of pending request */
		nb_pending_raw_data--;
		/* Check end of session */
		check_end_of_session();
		break;

	default:
		break;
	}
}

static void push_data(struct stored_data *ptr, uint32_t data_len)
{
	uint8_t datasize = offsetof(struct stored_data, data) + data_len;
	struct stored_data *data_to_save = balloc(RAW_STORAGE_ELT_SIZE, NULL);

	/* Only copy the relevant part of the structure */
	memcpy(data_to_save, ptr, datasize);

	/* Update the size in the structure to save in the NVM */
	data_to_save->datasize = datasize;

	circular_storage_service_push(circular_storage_service_conn,
				      (void *)data_to_save,
				      storage,
				      data_to_save);
}

/* Aggregate data sensor and push them */
static void aggregate_and_dump(sensor_service_subscribe_data_event_t *p_evt)
{
	sensor_service_sensor_data_header_t *p_data_header =
		&p_evt->sensor_data_header;
	uint8_t type = GET_SENSOR_TYPE(p_evt->handle);
	uint8_t data_len = p_data_header->data_length;

	if (session_running && storage) {
		/* data_index is null when it is the first element */
		if (!data_index) {
			tmp_data.timestamp = p_data_header->timestamp;
			/* if timestamp change or there is not enough space to set sensor data:
			 * push data and reset timestamp and data_index */
		} else if ((p_data_header->timestamp >
			    (tmp_data.timestamp + (sampling_interval / 2))) ||
			   ((data_index + DATA_HEADER_SIZE + data_len) >
			    sizeof(tmp_data.data))) {
			push_data(&tmp_data, data_index);
			data_index = 0;
			tmp_data.timestamp = p_data_header->timestamp;
		}
		/* Complete data header */
		tmp_data.data[data_index] = type;
		tmp_data.data[data_index + 1] = data_len;
		/* Copy sensor data */
		memcpy(&tmp_data.data[data_index + DATA_HEADER_SIZE],
		       p_data_header->data,
		       data_len);
		/* Increment data_index*/
		data_index += data_len + DATA_HEADER_SIZE;
	}
}

/* handle for sensors data */
static void handle_sensor_subscribe_data(struct cfw_message *msg)
{
	sensor_service_subscribe_data_event_t *p_evt =
		(sensor_service_subscribe_data_event_t *)msg;

	aggregate_and_dump(p_evt);
}

static void handle_start_scanning_evt(struct cfw_message *msg)
{
	sensor_service_scan_event_t *p_evt = (sensor_service_scan_event_t *)msg;
	sensor_service_on_board_scan_data_t on_board_data =
		p_evt->on_board_data;
	uint8_t sensor_type = p_evt->sensor_type;

	handles[sensor_type] = GET_SENSOR_HANDLE(sensor_type,
						 on_board_data.ch_id);
}

static void handle_msg(struct cfw_message *msg, void *data)
{
	switch (CFW_MESSAGE_ID(msg)) {
	case MSG_ID_SENSOR_SERVICE_SUBSCRIBE_DATA_RSP:;
		uint8_t status =
			((sensor_service_message_general_rsp_t *)msg)->status;
		uint8_t response = TOPIC_STATUS_OK;
		nb_subscribe_rsp++;
		if (status != RESP_SUCCESS) {
			response = TOPIC_STATUS_FAIL;
			/* If a susbscribe failed do not wait for any more responses */
			nb_subscribe_rsp = nb_subscribe_expected;
		}

		if (nb_subscribe_rsp == nb_subscribe_expected) {
			raw_sensor_streaming_iq_send_itm_response(response);
			/* To make sure we don't send any more responses */
			nb_subscribe_rsp++;
		}
		break;
	case MSG_ID_SENSOR_SERVICE_START_SCANNING_EVT:
		handle_start_scanning_evt(msg);
		break;
	case MSG_ID_SENSOR_SERVICE_SUBSCRIBE_DATA_EVT:
		/* Treat the data even if session is in progress */
		if (session_running)
			handle_sensor_subscribe_data(msg);
		break;
	case MSG_ID_CIRCULAR_STORAGE_SERVICE_PUSH_RSP:
		bfree(CFW_MESSAGE_PRIV(msg));
		if (((circular_storage_service_push_rsp_msg_t *)msg)->status !=
		    DRV_RC_OK)
			pr_error(LOG_MODULE_MAIN, "Raw data write failure [%d]",
				 ((circular_storage_service_push_rsp_msg_t *)
				  msg)->status);
		/* Peek the data even if session is in progress */
		else if (buffer_empty && session_running && use_stream) {
			buffer_empty = false;
			circular_storage_service_peek(
				circular_storage_service_conn,
				storage,
				NULL);
		}
		break;
	case MSG_ID_CIRCULAR_STORAGE_SERVICE_GET_RSP:;
		circular_storage_service_get_rsp_msg_t *init_resp =
			(circular_storage_service_get_rsp_msg_t *)msg;
		if (init_resp->status == DRV_RC_OK)
			storage = init_resp->storage;
		else
			pr_error(LOG_MODULE_MAIN,
				 "Circular storage get failure [%d]",
				 init_resp->status);
		break;
	case MSG_ID_CIRCULAR_STORAGE_SERVICE_CLEAR_RSP:
		if (CFW_MESSAGE_PRIV(msg)) {
			void (*start_sensors)(struct
					      sensor_subscribe_parameters) =
				CFW_MESSAGE_PRIV(msg);
			start_sensors(sensor_parameter);
		}
		break;
	case MSG_ID_CIRCULAR_STORAGE_SERVICE_PEEK_RSP:;
		circular_storage_service_peek_rsp_msg_t *peek_resp =
			(circular_storage_service_peek_rsp_msg_t *)msg;
		if (peek_resp->status == DRV_RC_OK) {
			struct stored_data *p_data = (void *)peek_resp->buffer;
			int rv;

			buffer_empty = false;
			rv = iasp_write(NULL, IASP_RAWDATA_CHANNEL,
					p_data,
					p_data->datasize, NULL, 0);
			if (rv >= 0) {
				circular_storage_service_clear(
					circular_storage_service_conn,
					storage, 1, NULL);

				/* Increase the number of BLE request */
				nb_pending_raw_data++;
			} else {
				pr_error(LOG_MODULE_MAIN,
					 "iasp_write failure [%d]", rv);
			}
			if ((nb_pending_raw_data < RAWDATA_IASP_MAX_MSGS) &&
			    con_opened) {
				/* try to push more data */
				circular_storage_service_peek(
					circular_storage_service_conn,
					storage,
					NULL);
			}
		} else {
			buffer_empty = true;
			/* Check end of session */
			check_end_of_session();
		}

		bfree(peek_resp->buffer);
		break;
	default: break;
	}
	cfw_msg_free(msg);
}


/* Start session subscribing to expected sensors */
static void start_session(struct sensor_subscribe_parameters parameters)
{
	uint8_t data_type = ACCEL_DATA;
	uint8_t i = 0;
	uint32_t tmp_mask = parameters.sensor_mask;

	/* Sampling interval = 1000 (ms) / frequency */
	sampling_interval = 1000 / parameters.frequency;
	/* Reporting interval is the minimum between:
	 * - Maximum expected latency
	 * - and time to have 5 samples (5 * sampling_interval) */
	uint8_t reporting_interval = MIN(sampling_interval * 5, MAXIMUM_LATENCY);

	pr_debug(LOG_MODULE_MAIN,
		 "START RAW DATA SESSION - sensor_mask: %d, freq: %d",
		 parameters.sensor_mask,
		 parameters.frequency);

	nb_subscribe_expected = 0;
	nb_subscribe_rsp = 0;
	while (tmp_mask) {
		if ((tmp_mask & 1) && handles[i]) {
			sensor_service_subscribe_data(sensor_service_conn, NULL,
						      handles[i],
						      &data_type, 1,
						      parameters.frequency,
						      reporting_interval);
			pr_debug(LOG_MODULE_MAIN, "Sub %d", i);
			nb_subscribe_expected++;
		}
		i++;
		tmp_mask = sensor_parameter.sensor_mask >> i;
	}

	session_running = true;
	/* When starting the session the buffer is empty */
	buffer_empty = true;
}


/* Check expected sensors can be used */
bool rawdata_start(uint32_t sensor_mask, uint32_t frequency, bool use_streaming)
{
	/* Clear circular storage only if:
	 * sensors and circular storage are successfully initialized and
	 * session is not running and
	 * BLE connection is open if streaming is used */
	if (!session_running && storage && (con_opened || !use_streaming)) {
		/* Store the subscribe parameters */
		sensor_parameter.sensor_mask = sensor_mask;
		sensor_parameter.frequency = frequency;

		uint8_t i = 0;
		uint32_t tmp_mask = sensor_mask;
		while (tmp_mask) {
			if ((tmp_mask & 1) && !handles[i]) {
				pr_error(LOG_MODULE_MAIN, "Invalid sensor %d",
					 i);
				raw_sensor_streaming_iq_send_itm_response(
					TOPIC_STATUS_FAIL);
				return false;
			}
			i++;
			tmp_mask = sensor_mask >> i;
		}

		circular_storage_service_clear(circular_storage_service_conn,
					       storage, 0, start_session);
		if (use_streaming) {
			struct bt_le_conn_param con_params = { 8, 16, 0, 100 };
			/* Speed up the connection before starting the streaming */
			ble_app_conn_update(&con_params);
		}
		use_stream = use_streaming;
		return true;
	}
	raw_sensor_streaming_iq_send_itm_response(
		TOPIC_STATUS_FAIL);
	return false;
}

/* Stop raw data sensor collection unsubscribing to sensors */
bool rawdata_end(void)
{
	uint8_t data_type = ACCEL_DATA;
	uint8_t i = 0;
	uint32_t tmp_mask = sensor_parameter.sensor_mask;

	if (session_running) {
		/* Send last saved data and reset variables */
		if (data_index) {
			push_data(&tmp_data, data_index);
			/* Reset data_index value */
			data_index = 0;
		}
		while (tmp_mask) {
			if ((tmp_mask & 1) && handles[i]) {
				pr_debug(LOG_MODULE_MAIN, "Unsub %d", i);
				sensor_service_unsubscribe_data(
					sensor_service_conn, NULL,
					handles[i],
					&data_type, 1);
			}
			i++;
			tmp_mask = sensor_parameter.sensor_mask >> i;
		}

		session_running = false;
		if (!use_stream) {
			pr_info(LOG_MODULE_MAIN, "Raw data session is over");
			raw_sensor_streaming_iq_send_itm_response(
				TOPIC_STATUS_OK);
		} else {
			/* Check end of session */
			check_end_of_session();
		}

		pr_debug(LOG_MODULE_MAIN, "STOPPING RAW DATA SESSION");
		return true;
	}
	raw_sensor_streaming_iq_send_itm_response(
		TOPIC_STATUS_FAIL);
	return false;
}


static void service_connection_cb(cfw_service_conn_t *handle, void *param)
{
	if ((void *)CIRCULAR_STORAGE_SERVICE_ID == param) {
		circular_storage_service_conn = handle;
		circular_storage_service_get(circular_storage_service_conn,
					     RAW_STORAGE_KEY, NULL);
	} else {
		/* ARC_SC_SVC_ID */
		memset(handles, 0, ON_BOARD_SENSOR_TYPE_END + 1);
		sensor_service_conn = handle;
		sensor_service_start_scanning(handle, NULL, ~(0));
	}
}

void rawdata_init(T_QUEUE queue)
{
	client = cfw_client_init(queue, handle_msg, NULL);

	/* Open the sensor service */
	cfw_open_service_helper(client, ARC_SC_SVC_ID,
				service_connection_cb, (void *)ARC_SC_SVC_ID);

	/* Open the circular_storage service */
	cfw_open_service_helper(client,
				CIRCULAR_STORAGE_SERVICE_ID,
				service_connection_cb,
				(void *)CIRCULAR_STORAGE_SERVICE_ID);

	/* Register IASP channel */
	iasp_register(&raw_data_iasp);

	/* Set callback for IQ */
	raw_sensor_streaming_iq_set_start_session_cb(rawdata_start);
	raw_sensor_streaming_iq_set_stop_session_cb(rawdata_end);
}
