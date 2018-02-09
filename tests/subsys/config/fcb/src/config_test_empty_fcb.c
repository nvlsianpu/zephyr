/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_fcb.h"

void config_test_empty_fcb(void)
{
	int rc;
	struct conf_fcb cf;

	config_wipe_srcs();
	config_wipe_fcb(fcb_sectors, ARRAY_SIZE(fcb_sectors));

	cf.cf_fcb.f_magic = CONFIG_CFGSYS_FCB_MAGIC;
	cf.cf_fcb.f_sectors = fcb_sectors;
	cf.cf_fcb.f_sector_cnt = ARRAY_SIZE(fcb_sectors);

	rc = conf_fcb_src(&cf);
	zassert_true(rc == 0, "conf_fcb_src call should succeed\n");

	/*
	 * No values
	 */
	conf_load();

	config_wipe_srcs();
	ctest_clear_call_state();
}
