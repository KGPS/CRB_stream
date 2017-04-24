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


#ifndef __RAWDATA_H__
#define __RAWDATA_H__

#include "os/os.h"
#include "util/misc.h"
/* Main sensors API */
#include "services/sensor_service/sensor_service.h"

#define RAW_STORAGE_KEY      GEN_KEY('S', 'R', 'A', 'W')
/* Max size of the stored element */
#define RAW_STORAGE_ELT_SIZE 128

#define DEFAULT_MASK         ACCEL_TYPE_MASK | GYRO_TYPE_MASK
#define DEFAULT_FREQ         100

/** Raw Data sensor Collection init.
 * This will start the required services and start sensor scanning.
 *
 * @param queue message queue to be used
 */
void rawdata_init(T_QUEUE queue);

/** Raw Data sensor Collection start.
 * This will subscribe to accel and gyro events.
 *
 * @param sensor_mask list of sensors to activate
 * @param frequency sampling rate frequency
 * @param use_streaming true if streaming is used, false otherwise
 * @return true if sensors are started, false otherwise
 */
bool rawdata_start(uint32_t sensor_mask, uint32_t frequency, bool use_streaming);

/** Raw Data sensor Collection end.
 * This will unsubscribe to accel and gyro events.
 * @return true if sensors are stopped, false otherwise
 */
bool rawdata_end(void);

#endif
