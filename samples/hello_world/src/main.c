/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

#include <fs.h>
#include <ff.h>

/* FatFs work area */
static FATFS fat_fs;
#define FATFS_MNTP	"/NAND:"

/* mounting info */
static struct fs_mount_t fatfs_mnt = {
	.type = FS_FATFS,
	.mnt_point = FATFS_MNTP,
	.fs_data = &fat_fs,
};

static void test_mount(void)
{
	int res;

	res = fs_mount(&fatfs_mnt);
	if (res < 0) {
		printk("Error mounting fs [%d]\n", res);
	} else {
		printk("Success mounting fs [%d]\n", res);
	}
}

void main(void)
{
	printk("Hello World! %s\n", CONFIG_ARCH);
	test_mount();
}
