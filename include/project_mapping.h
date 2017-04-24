/**
 * Allow project to override this partition scheme
 * The following variables are allowed to be defined:
 *
 * QUARK_START_PAGE the first page where the QUARK code is located
 * QUARK_NB_PAGE the number of pages reserved for the QUARK. The ARC gets the
 *               remaining pages (out of 148).
 */

#include "machine/soc/intel/quark_se/quark_se_mapping.h"

/* Partition used for PVP events storage - 3 blocks = 12 kB */
#define SPI_PVP_EVENTS_PARTITION_ID                     5
#define SPI_PVP_EVENTS_FLASH_ID                         SERIAL_FLASH_ID
#define SPI_PVP_EVENTS_START_BLOCK                      0
#define SPI_PVP_EVENTS_END_BLOCK                        2
#define SPI_PVP_EVENTS_NB_BLOCKS                        ( \
		(SPI_PVP_EVENTS_END_BLOCK - \
		 SPI_PVP_EVENTS_START_BLOCK) + 1)

/* Partition used for Raw data collection storage - 506 blocks = 2 MB */
#define SPI_RAWDATA_COLLECTION_PARTITION_ID             6
#define SPI_RAWDATA_COLLECTION_FLASH_ID                 SERIAL_FLASH_ID
#define SPI_RAWDATA_COLLECTION_START_BLOCK \
	(SPI_PVP_EVENTS_END_BLOCK + 1)
#define SPI_RAWDATA_COLLECTION_END_BLOCK                ( \
		SPI_SYSTEM_EVENT_START_BLOCK - 1)
#define SPI_RAWDATA_COLLECTION_NB_BLOCKS                ( \
		(SPI_RAWDATA_COLLECTION_END_BLOCK - \
		 SPI_RAWDATA_COLLECTION_START_BLOCK) + 1)

#undef NUMBER_OF_PARTITIONS
#define NUMBER_OF_PARTITIONS                    8
