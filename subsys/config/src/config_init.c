/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <errno.h>

#include "config/config.h"
#include "config/config_file.h"
#include <zephyr.h>

#ifdef CONFIG_CFGSYS_FS
#include <fs.h>

static struct conf_file config_init_conf_file = {
	.cf_name = CONFIG_CFGSYS_FS_FILE,
	.cf_maxlines = CONFIG_CFGSYS_FS_MAX_LINES
};

static void config_init_fs(void)
{
	int rc;

	rc = conf_file_src(&config_init_conf_file);
	if (rc) {
		k_panic();
	}

	rc = conf_file_dst(&config_init_conf_file);
	if (rc) {
		k_panic();
	}
}

#elif defined(CONFIG_CFGSYS_FCB)
#include "fcb.h"
#include "config/config_fcb.h"

static struct flash_sector conf_fcb_area[CONFIG_CFGSYS_FCB_NUM_AREAS + 1];

static struct conf_fcb config_init_conf_fcb = {
	.cf_fcb.f_magic = CONFIG_CFGSYS_FCB_MAGIC,
	.cf_fcb.f_sectors = conf_fcb_area,
};

static void config_init_fcb(void)
{
	u32_t cnt;
	int rc;
	const struct flash_area *fap;

	rc = flash_area_get_sectors(CONFIG_CFGSYS_FCB_FLASH_AREA, &cnt,
								conf_fcb_area);
	if (rc != 0) {
		k_panic();
	}

	config_init_conf_fcb.cf_fcb.f_sector_cnt = cnt;

	rc = conf_fcb_src(&config_init_conf_fcb);

	if (rc != 0) {
		k_panic();
	}

	rc = flash_area_open(CONFIG_CFGSYS_FCB_FLASH_AREA, &fap);

	if (rc == 0) {
		rc = flash_area_erase(fap, 0, fap->fa_size);
		flash_area_close(fap);
	}

	if (rc != 0) {
		k_panic();
	} else {
		rc = conf_fcb_src(&config_init_conf_fcb);
	}

	rc = conf_fcb_dst(&config_init_conf_fcb);

	if (rc != 0) {
		k_panic();
	}
}

#endif

void config_pkg_init(void)
{
	/* Ensure this function only gets called by sysinit. */
	/*SYSINIT_ASSERT_ACTIVE();*/

	conf_init();

#ifdef CONFIG_CFGSYS_FS
	config_init_fs();
#elif defined(CONFIG_CFGSYS_FCB)
	config_init_fcb();
#endif
}

void
config_pkg_init_stage2(void)
{
	/*
	 * Must be called after root FS has been initialized.
	 */
#ifdef CONFIG_CFGSYS_FS
	fs_mkdir(CONFIG_CFGSYS_FS_DIR);
#endif
}
