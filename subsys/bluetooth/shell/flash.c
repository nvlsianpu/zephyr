/** @file
 * @brief Bluetooth Controller and flash coopertion
 *
 */

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <shell/shell.h>
#include <misc/printk.h>

#include <stdlib.h>
#include <string.h>
#include "flash.h"
#include <soc.h>

#define FLASH_SHELL_MODULE "flash"
#define BUF_ARRAY_CNT 16


static int cmd_erase(int argc, char *argv[])
{
	struct device *flash_dev;
	flash_dev = device_get_binding(CONFIG_SOC_FLASH_NRF5_DEV_NAME);

	if (!flash_dev) {
		printk("Nordic nRF5 flash driver was not found!\n");
		return -1;
	}

        u32_t page_addr = strtoul(argv[1], NULL, 16);
        u32_t size;

	if (argc > 2) {
		size = strtoul(argv[2], NULL, 16);
        } else {
                size = NRF_FICR->CODEPAGESIZE;;
        }

	flash_write_protection_set(flash_dev, 0);


	int result;

	result = flash_erase(flash_dev, page_addr, size);

        if (result) {
	        printk("Erase falied, code %u\n",result);
        } else {
                printk("Erase succed\n");
        }

	return result;
}


static int cmd_flash(int argc, char *argv[])
{
	struct device *flash_dev;
	flash_dev = device_get_binding(CONFIG_SOC_FLASH_NRF5_DEV_NAME);

	if (!flash_dev) {
		printk("Nordic nRF5 flash driver was not found!\n");
		return -1;
	}

	u32_t w_addr = strtoul(argv[1], NULL, 16);


	if (argc <= 2) {
		printk("Type data to be writen!\n");
		return -1;
        }

	u32_t buf_array[BUF_ARRAY_CNT];
	u32_t check_array[BUF_ARRAY_CNT];
	int j = 0;

	for (int i = 2; i < argc && i < BUF_ARRAY_CNT; i++) {
		buf_array[j]   =  strtoul(argv[i], NULL, 16);
		check_array[j] = ~buf_array[j];
		j++;
        }

	flash_write_protection_set(flash_dev, 0);

	if (flash_write(flash_dev,w_addr, buf_array,
		sizeof(buf_array[0] * j)) != 0) {
		printk("Write internal ERROR!\n");
		return -1;
	}

	flash_read(flash_dev, w_addr, check_array, sizeof(buf_array[0] * j));

	if (memcmp(buf_array, check_array, sizeof(buf_array[0] * j)) == 0) {
		printk("Write OK.\n");
	} else {
		printk("Write check ERROR!\n");
		return -1;
	}

	return 0;
}

static const struct shell_cmd flash_commands[] = {
	{ "flash-write", cmd_flash, "<address> <Byte> <Byte>..."},
        { "flash-erase", cmd_erase, "<page address> <size>"},
	{ NULL, NULL, NULL}
};

SHELL_REGISTER(FLASH_SHELL_MODULE, flash_commands);
