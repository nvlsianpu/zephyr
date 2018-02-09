/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_fcb.h"

#ifdef TEST_LONG
#define TESTS_S2_FCB_ITERATIONS 32
#else
#define TESTS_S2_FCB_ITERATIONS 2
#endif

void config_test_save_2_fcb(void)
{
	int rc;
	struct conf_fcb cf;

	int i;

	config_wipe_srcs();

	cf.cf_fcb.f_magic = CONFIG_CFGSYS_FCB_MAGIC;
	cf.cf_fcb.f_sectors = fcb_sectors;
	cf.cf_fcb.f_sector_cnt = ARRAY_SIZE(fcb_sectors);

	rc = conf_fcb_src(&cf);
	zassert_true(rc == 0, "can't register FCB as configuration source\n");

	rc = conf_fcb_dst(&cf);
	zassert_true(rc == 0,
		     "can't register FCB as configuration destination\n");

	config_test_fill_area(test_ref_value, 0);
	memcpy(val_string, test_ref_value, sizeof(val_string));

	val8 = 42;
	rc = conf_save();
	zassert_true(rc == 0, "fcb write error\n");

	val8 = 0;
	memset(val_string[0], 0, sizeof(val_string[0]));
	rc = conf_load();
	zassert_true(rc == 0, "fcb read error\n");
	zassert_true(val8 == 42, "bad value read\n");
	zassert_true(!strcmp(val_string[0], test_ref_value[0]),
		     "bad value read\n");
	test_export_block = 1;

	/*
	 * Now add the number of settings to max. Keep adjusting the test_data,
	 * check that rollover happens when it's supposed to.
	 */
	c2_var_count = 64;

	for (i = 0; i < TESTS_S2_FCB_ITERATIONS; i++) {
		config_test_fill_area(test_ref_value, i);
		memcpy(val_string, test_ref_value, sizeof(val_string));

		rc = conf_save();
		zassert_true(rc == 0, "fcb write error\n");

		memset(val_string, 0, sizeof(val_string));

		val8 = 0;
		rc = conf_load();
		zassert_true(rc == 0, "fcb read error\n");
		zassert_true(!memcmp(val_string, test_ref_value,
				     sizeof(val_string)),
			     "bad value read\n");
		zassert_true(val8 == 42, "bad value read\n");
	}
	c2_var_count = 0;
}
