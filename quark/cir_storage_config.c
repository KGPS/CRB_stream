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

#include <misc/util.h>
#include "project_mapping.h"
#include "cfw/cfw.h"
#include "services/circular_storage_service/circular_storage_service.h"
#include "rawdata.h"
#include "iq/init_iq.h"

enum {
	RAW_CONFIGURATION,
	PVP_EVENTS_CONFIGURATION,
	CIR_STORAGE_CONFIG_COUNT
};

/* Flash memory partitioning array */
extern flash_partition_t storage_configuration[];

struct circular_storage_configuration cir_storage_config = {
	.cir_storage_list = (struct cir_storage[]) {
		[RAW_CONFIGURATION] = {
			.key = RAW_STORAGE_KEY,
			.partition_id = SPI_RAWDATA_COLLECTION_PARTITION_ID,
			.first_block = SPI_RAWDATA_COLLECTION_START_BLOCK,
			.block_count = SPI_RAWDATA_COLLECTION_NB_BLOCKS,
			.element_size = RAW_STORAGE_ELT_SIZE,
		},
		[PVP_EVENTS_CONFIGURATION] = {
			.key = PVP_STORAGE_KEY,
			.partition_id = SPI_PVP_EVENTS_PARTITION_ID,
			.first_block = SPI_PVP_EVENTS_START_BLOCK,
			.block_count = SPI_PVP_EVENTS_END_BLOCK,
			.element_size = PVP_STORAGE_ELT_SIZE,
		},
	},
	.cir_storage_count = CIR_STORAGE_CONFIG_COUNT,
	.partitions = storage_configuration,
	.no_part = NUMBER_OF_PARTITIONS,
};
