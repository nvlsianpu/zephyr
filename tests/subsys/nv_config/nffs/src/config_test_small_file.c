/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_file.h"

void config_test_small_file(void)
{
	int rc;
	struct conf_file cf_mfg;
	struct conf_file cf_running;
	const char cf_mfg_test[] = "myfoo/mybar=1";
	const char cf_running_test[] = " myfoo/mybar = 8 ";

	config_wipe_srcs();

	cf_mfg.cf_name = TEST_CONFIG_DIR "/mfg";
	cf_running.cf_name = TEST_CONFIG_DIR "/running";

	rc = conf_file_src(&cf_mfg);
	zassert_true(rc == 0, "can't register FS as configuration source\n");
	rc = conf_file_src(&cf_running);
	zassert_true(rc == 0, "can't register FS as configuration source\n");

	rc = fsutil_write_file(TEST_CONFIG_DIR "/mfg", cf_mfg_test,
			       sizeof(cf_mfg_test));
	zassert_true(rc == 0, "can't write to file\n");

	conf_load();
	zassert_true(test_set_called, "the SET handler wasn't called\n");
	zassert_true(val8 == 1,
		     "SET handler: was called with wrong parameters\n");

	ctest_clear_call_state();

	rc = fsutil_write_file(TEST_CONFIG_DIR "/running", cf_running_test,
			       sizeof(cf_running_test));
	zassert_true(rc == 0, "can't write to file\n");

	conf_load();
	zassert_true(test_set_called, "the SET handler wasn't called\n");
	zassert_true(val8 == 8,
		     "SET handler: was called with wrong parameters\n");

	ctest_clear_call_state();
}
