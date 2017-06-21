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

	if (argc < 2) {
		printk("Page address reqired!\n");
		return -1;
	}

	u32_t page_addr = strtoul(argv[1], NULL, 16);
	u32_t size;

	if (argc > 2) {
		size = strtoul(argv[2], NULL, 16);
	} else {
		size = NRF_FICR->CODEPAGESIZE;
	}

	flash_write_protection_set(flash_dev, 0);


	int result;

	result = flash_erase(flash_dev, page_addr, size);

	if (result) {
		printk("Erase Failed, code %d\n", result);
	} else {
		printk("Erase success\n");
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

	if (argc < 2) {
		printk("Address reqired!\n");
		return -1;
	}

	u32_t w_addr = strtoul(argv[1], NULL, 16);


	if (argc <= 2) {
		printk("Type data to be written!\n");
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

	if (flash_write(flash_dev, w_addr, buf_array,
		sizeof(buf_array[0]) * j) != 0) {
		printk("Write internal ERROR!\n");
		return -1;
	}

	printk("Write OK.\n");

	flash_read(flash_dev, w_addr, check_array, sizeof(buf_array[0]) * j);

	if (memcmp(buf_array, check_array, sizeof(buf_array[0]) * j) == 0) {
		printk("Verified.\n");
	} else {
		printk("Verification ERROR!\n");
		return -1;
	}

	return 0;
}

static int cmd_read(int argc, char *argv[])
{
	struct device *flash_dev;

	flash_dev = device_get_binding(CONFIG_SOC_FLASH_NRF5_DEV_NAME);

	if (!flash_dev) {
		printk("Nordic nRF5 flash driver was not found!\n");
		return -1;
	}

	if (argc < 2) {
		printk("Address reqired!\n");
		return -1;
	}

	u32_t addr = strtoul(argv[1], NULL, 16);
	int cnt;

	if (argc > 2) {
		cnt = strtoul(argv[2], NULL, 16);
	} else {
		cnt = 1;
	}

	while (cnt--) {
		u32_t data;

		flash_read(flash_dev, addr, &data, sizeof(data));
		printk("0x%x ", data);
		addr += sizeof(data);
	}

	printk("\n");

	return 0;
}

static int cmd_test(int argc, char *argv[])
{
	struct device *flash_dev;

	flash_dev = device_get_binding(CONFIG_SOC_FLASH_NRF5_DEV_NAME);

	if (!flash_dev) {
		printk("Nordic nRF5 flash driver was not found!\n");
		return -1;
	}

	if (argc != 4) {
		printk("3 paramiters reqired!\n");
		return -1;
	}

	u32_t addr = strtoul(argv[1], NULL, 16);
	u32_t size = strtoul(argv[2], NULL, 16);
	u32_t repeat = strtoul(argv[3], NULL, 16);

	flash_write_protection_set(flash_dev, 0);


	int result;
	u8_t arr[size];

	for (u32_t i = 0; i < size; i++) {
		arr[i] = (u8_t) i;
	}


	while (repeat--) {
		result = flash_erase(flash_dev, addr, size);

		if (result) {
			printk("Erase Failed, code %d\n", result);
			return -1;
		}

		printk("Erase OK.\n");

		if (flash_write(flash_dev, addr, arr, size) != 0) {
			printk("Write internal ERROR!\n");
			return -1;
		}

		printk("Write OK.\n");

	}

	printk("Erase-Write test done.\n");

	return 0;
}

static const struct shell_cmd flash_commands[] = {
	{ "flash-write", cmd_flash, "<address> <Dword> <Byte>..."},
	{ "flash-erase", cmd_erase, "<page address> <size>"},
	{ "flash-read", cmd_read, "<address> <Dword count>"},
	{ "flash-test", cmd_test, "<address> <size> <repeat count>"},
	{ NULL, NULL, NULL}
};

SHELL_REGISTER(FLASH_SHELL_MODULE, flash_commands);
