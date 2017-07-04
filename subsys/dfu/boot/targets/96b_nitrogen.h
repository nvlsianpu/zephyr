/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2017 Linaro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Bootloader device specific configuration.
 */

#define FLASH_DRIVER_NAME		CONFIG_SOC_FLASH_NRF5_DEV_NAME
#define FLASH_ALIGN			4
#define FLASH_AREA_IMAGE_0_OFFSET	0x08000
#define FLASH_AREA_IMAGE_0_SIZE		0x34000
#define FLASH_AREA_IMAGE_1_OFFSET	0x3C000
#define FLASH_AREA_IMAGE_1_SIZE		0x34000
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET	0x70000
#define FLASH_AREA_IMAGE_SCRATCH_SIZE	0x0D000
/* Flash sector size is provided by SoC family include */
