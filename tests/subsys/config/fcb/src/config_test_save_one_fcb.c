/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_fcb.h"

void config_test_save_one_fcb(void)
{
	int rc;
	struct conf_fcb cf;

	config_wipe_srcs();
	config_wipe_fcb(fcb_sectors, ARRAY_SIZE(fcb_sectors));

	cf.cf_fcb.f_magic = CONFIG_CFGSYS_FCB_MAGIC;
	cf.cf_fcb.f_sectors = fcb_sectors;
	cf.cf_fcb.f_sector_cnt = ARRAY_SIZE(fcb_sectors);

	rc = conf_fcb_src(&cf);
	zassert_true(rc == 0, "can't register FCB as configuration source\n");

	rc = conf_fcb_dst(&cf);
	zassert_true(rc == 0,
			 "can't register FCB as configuration destination\n");

	val8 = 33;
	rc = conf_save();
	zassert_true(rc == 0, "fcb write error\n");

	rc = conf_save_one("myfoo/mybar", "42");
	zassert_true(rc == 0, "fcb one item write error\n");

	rc = conf_load();
	zassert_true(rc == 0, "fcb read error\n");
	zassert_true(val8 == 42, "bad value read\n");

	rc = conf_save_one("myfoo/mybar", "44");
	zassert_true(rc == 0, "fcb one item write error\n");

	rc = conf_load();
	zassert_true(rc == 0, "fcb read error\n");
	zassert_true(val8 == 44, "bad value read\n");
}
