/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *  Copyright (C) 2017, Linaro Ltd
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#define FLASH_DRIVER_NAME		CONFIG_SOC_FLASH_NRF5_DEV_NAME
#define FLASH_ALIGN			4
#define FLASH_AREA_IMAGE_0_OFFSET	0x08000
#define FLASH_AREA_IMAGE_0_SIZE		0x6C000
#define FLASH_AREA_IMAGE_1_OFFSET	0x74000
#define FLASH_AREA_IMAGE_1_SIZE		0x6C000
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET	0xE0000
#define FLASH_AREA_IMAGE_SCRATCH_SIZE	0x1D000
/* Flash sector size is provided by SoC include */
