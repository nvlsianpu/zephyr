/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2016 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Headers target/devboard.h are copies of board headers form mcuboot sources */
/* see <mcuboot>/boot/zephyr/targets/                                         */

#ifndef __MCUBOOT_CONSTRAINS_H__
#define __MCUBOOT_CONSTRAINS_H__

/* Flash specific configs */
#if defined(CONFIG_SOC_SERIES_NRF52X)
#include <soc.h>
#define FLASH_STATE_SIZE (NRF_FICR->CODESIZE - FLASH_STATE_OFFSET)
#if defined(CONFIG_BOARD_96B_NITROGEN)
#include "targets/96b_nitrogen.h"
#elif defined(CONFIG_BOARD_NRF52840_PCA10056)
#include "targets/nrf52840_pca10056.h"
#else
#error Unknown NRF52X board
#endif	/* CONFIG_BOARD_96B_NITROGEN */
#else
#error Unknown SoC family
#endif /* CONFIG_SOC_SERIES_NRF52X */

#define FLASH_MIN_WRITE_SIZE FLASH_ALIGN
#define FLASH_BANK0_OFFSET FLASH_AREA_IMAGE_0_OFFSET
#define FLASH_BANK_SIZE FLASH_AREA_IMAGE_0_SIZE
#define FLASH_BANK1_OFFSET FLASH_AREA_IMAGE_1_OFFSET
#define FLASH_STATE_OFFSET (FLASH_IMAGE_SCRATCH_OFFSET +\
			    FLASH_IMAGE_SCRATCH_SIZE)

#endif	/* __MCUBOOT_CONSTRAINS_H__ */
