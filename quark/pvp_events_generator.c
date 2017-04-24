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

#include "os/os.h"
#include "infra/log.h"

#include "cfw/cfw.h"

/* Main sensors API */
#include "services/sensor_service/sensor_service.h"

#include "pvp_events_generator.h"
#include "iq/pvp_events_iq.h"
#include "lib/ble/pattern/ble_pattern.h"

/* Client */
static cfw_client_t *client = NULL;

/* Sensors client */
static cfw_service_conn_t *sensor_service_conn = NULL;

static sensor_service_t pvp_handle = NULL;

static void (*init_done_callback)(void) = NULL;

struct track_suitcase_events{
 uint8_t total_pattern_square;
}track_pattern_events_debug = {0};


/* handle for sensors data */
static void handle_sensor_subscribe_data(struct cfw_message *msg)
{
	sensor_service_subscribe_data_event_t *p_evt =
		(sensor_service_subscribe_data_event_t *)msg;
	uint8_t sensor_type = GET_SENSOR_TYPE(p_evt->handle);
	sensor_service_sensor_data_header_t *p_data_header =
		&p_evt->sensor_data_header;

	switch (sensor_type) {
	case SENSOR_ALGO_KB:;
		/* Push data */
		struct kb_result *p =
			(struct kb_result *)p_data_header->data;
		pr_info(LOG_MODULE_MAIN, "KB classifier=%d", p->nClassLabel);
		pvp_event_push_classifier(p->nClassLabel);
		if (p->nClassLabel==2)
		{
			track_pattern_events_debug.total_pattern_square++;
			ble_pattern_update(track_pattern_events_debug.total_pattern_square,PATTERN_SQUARE);
		}
		break;
	}
}

static void handle_start_scanning_evt(struct cfw_message *msg)
{
	sensor_service_scan_event_t *p_evt = (sensor_service_scan_event_t *)msg;
	sensor_service_on_board_scan_data_t on_board_data =
		p_evt->on_board_data;
	uint8_t sensor_type = p_evt->sensor_type;

	switch (sensor_type) {
	case SENSOR_ALGO_KB:
		pvp_handle = GET_SENSOR_HANDLE(sensor_type,
					       on_board_data.
					       ch_id);

		pvp_events_iq_set_start_cb(pvp_events_generator_start);
		pvp_events_iq_set_end_cb(pvp_events_generator_end);
		if (init_done_callback) {
			init_done_callback();
		}
		break;
	}
}

static void handle_msg(struct cfw_message *msg, void *data)
{
	switch (CFW_MESSAGE_ID(msg)) {
	case MSG_ID_SENSOR_SERVICE_START_SCANNING_EVT:
		handle_start_scanning_evt(msg);
		break;
	case MSG_ID_SENSOR_SERVICE_SUBSCRIBE_DATA_EVT:
		handle_sensor_subscribe_data(msg);
		break;
	default: break;
	}
	cfw_msg_free(msg);
}

static void service_connection_cb(cfw_service_conn_t *handle, void *param)
{
	sensor_service_conn = handle;
	sensor_service_start_scanning(handle, NULL, ALGO_KB_MASK);
}

void pvp_events_generator_start(void)
{
	uint8_t data_type = ACCEL_DATA;

	if (pvp_handle)
		sensor_service_subscribe_data(sensor_service_conn, NULL,
					      pvp_handle,
					      &data_type, 1, 100,
					      10);
}

void pvp_events_generator_end(void)
{
	uint8_t data_type = ACCEL_DATA;

	if (pvp_handle)
		sensor_service_unsubscribe_data(sensor_service_conn, NULL,
						pvp_handle, &data_type, 1);
}

void pvp_events_generator_init(T_QUEUE queue, void (*init_done_cb)(void))
{
	client = cfw_client_init(queue, handle_msg, NULL);

	init_done_callback = init_done_cb;

	/* Open the sensor service */
	cfw_open_service_helper(client, ARC_SC_SVC_ID,
				service_connection_cb, (void *)ARC_SC_SVC_ID);
}
