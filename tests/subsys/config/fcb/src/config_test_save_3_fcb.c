/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_fcb.h"

#ifdef TEST_LONG
#define TESTS_S3_FCB_ITERATIONS 4096
#else
#define TESTS_S3_FCB_ITERATIONS 100
#endif

void config_test_save_3_fcb(void)
{
	int rc;
	struct conf_fcb cf;
	int i;

	config_wipe_srcs();
	config_wipe_fcb(fcb_sectors, ARRAY_SIZE(fcb_sectors));

	cf.cf_fcb.f_magic = CONFIG_CFGSYS_FCB_MAGIC;
	cf.cf_fcb.f_sectors = fcb_sectors;
	cf.cf_fcb.f_sector_cnt = 4;

	rc = conf_fcb_src(&cf);
	zassert_true(rc == 0, "can't register FCB as configuration source\n");

	rc = conf_fcb_dst(&cf);
	zassert_true(rc == 0,
		     "can't register FCB as configuration destination\n");

	for (i = 0; i < TESTS_S3_FCB_ITERATIONS; i++) {
		val32 = i;

		rc = conf_save();
		zassert_true(rc == 0, "fcb write error\n");

		val32 = 0;

		rc = conf_load();
		zassert_true(rc == 0, "fcb read error\n");
		zassert_true(val32 == i, "bad value read\n");
	}
}
