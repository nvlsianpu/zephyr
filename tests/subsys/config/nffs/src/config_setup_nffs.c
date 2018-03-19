/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include <device.h>
#include <fs.h>

#if 0
/* NFFS work area strcut */
static struct nffs_flash_desc flash_desc;

/* mounting info */
static struct fs_mount_t nffs_mnt = {
	.type = FS_NFFS,
	.mnt_point = TEST_FS_MPTR,
	.fs_data = &flash_desc,
};
#endif
/* FatFs work area */
static FATFS fat_fs;

/* mounting info */
static struct fs_mount_t fatfs_mnt = {
	.type = FS_FATFS,
	.mnt_point = TEST_FS_MPTR,
	.fs_data = &fat_fs
};

void config_setup_nffs(void)
{
	struct device *flash_dev;
	int rc;
#if 0
	flash_dev = device_get_binding(CONFIG_FS_NFFS_FLASH_DEV_NAME);
	zassert_not_null(flash_dev, "Can't bind to the flash device\n");

	/* set backend storage dev */
	nffs_mnt.storage_dev = flash_dev;

	rc = fs_mount(&nffs_mnt);
	zassert_true(rc == 0, "mounting nffs [%d]\n", rc);
#endif
	flash_dev = device_get_binding(FLASH_DEV_NAME);
	zassert_not_null(flash_dev, "Can't bind to the flash device\n");

	/* set backend storage dev */
	fatfs_mnt.storage_dev = flash_dev;
	rc = fs_mount(&fatfs_mnt);
	zassert_true(rc == 0, "mounting nffs [%d]\n", rc);

	rc = fs_unlink(TEST_CONFIG_DIR);
	zassert_true(rc == 0 || rc == -ENOENT,
		      "can't delete config directory%d\n", rc);
}
