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

#include <stdint.h>

#include "infra/log.h"
#include "infra/bsp.h"
#include "infra/port.h"
#include "infra/reboot.h"
#include "infra/wdt_helper.h"
#include "infra/xloop.h"

#include "cfw/cfw.h"

#include "services/storage_task.h"

/* UI Services configuration and helper */
#include "ui_config.h"

/* IQs */
#include "iq/init_iq.h"

/* BLE services helper */
#include "lib/ble/ble_app.h"

/* Raw Data sensor collection */
#include "rawdata.h"

/* Ispp init */
#include "ble_ispp.h"

/* PVP events */
#include "pvp_events_generator.h"

/* System main queue it will be used on the component framework to add messages
 * on it. */
static T_QUEUE queue;

static xloop_t loop;

/** Client message handler for main application */
static void client_message_handler(struct cfw_message *msg, void *param)
{
	switch (CFW_MESSAGE_ID(msg)) {
	case MSG_ID_CFW_SVC_AVAIL_EVT:
		break;
	case MSG_ID_UI_SERVICE_BTN_SINGLE_EVT: {
		union ui_service_drv_evt *broadcast_evt =
			(union ui_service_drv_evt *)msg;
		/* param is 0 if press is short, positive if press is long */
		if (broadcast_evt->btn_evt.param == 1)
			shutdown(SHUTDOWN_DEFAULT);
		else if (broadcast_evt->btn_evt.param == 2)
			ble_app_clear_bonds();
		break;
	}
	}
}

int wdt_func(void *param)
{
	/* Acknowledge the system watchdog to prevent panic and reset */
	wdt_keepalive();
	return 0;
}

void ble_app_ready(void)
{
	ble_ispp_init();
}

static void pvp_event_generator_initialized(void)
{
	pvp_events_generator_start();
}

/* Application main entry point */
void main_task(void *param)
{
	bool factory_mode = *(bool *)param;

	/* Init BSP (also init BSP on ARC core) */
	queue = bsp_init();

	pr_info(LOG_MODULE_MAIN, "BSP init done");

	/* start Quark watchdog */
	wdt_start(WDT_MAX_TIMEOUT_MS);

	/* Create storage task and tell storage-related services to run on it */
	init_storage();

	/* Init the CFW */
	cfw_init(queue);
	pr_info(LOG_MODULE_MAIN, "CFW init done");

	/* Init IQs before services to make sure that the user events IQ module
	 * is the first to subscribe to button press events when services are available. */
	if (!factory_mode)
		init_iqs(queue);

	ble_start_app(queue);

	/* UI service startup*/
	/* Create a client for using the UI service */
	cfw_client_t *client = cfw_client_init(queue, client_message_handler,
					       NULL);
	ui_start_helper(client);
	pr_info(LOG_MODULE_MAIN, "%s service init in progress...", "UI");

	/* Raw Data sensor collection initialization */
	rawdata_init(queue);

	/* PVP events initialization */
	pvp_events_generator_init(queue, pvp_event_generator_initialized);

	xloop_init_from_queue(&loop, queue);

	pr_info(LOG_MODULE_MAIN, "Quark go to main loop");

	xloop_post_func_periodic(&loop, wdt_func, NULL, WDT_MAX_TIMEOUT_MS / 2);
	xloop_run(&loop);
}
