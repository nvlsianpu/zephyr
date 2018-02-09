/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_file.h"

void config_test_save_in_file(void)
{
	int rc;
	struct conf_file cf;

	config_wipe_srcs();

	rc = fs_mkdir(TEST_CONFIG_DIR);
	zassert_true(rc == 0 || rc == -EEXIST, "can't create directory\n");

	cf.cf_name = TEST_CONFIG_DIR "/blah";
	rc = conf_file_src(&cf);
	zassert_true(rc == 0, "can't register FS as configuration source\n");

	rc = conf_file_dst(&cf);
	zassert_true(rc == 0,
		     "can't register FS as configuration destination\n");

	val8 = 8;
	rc = conf_save();
	zassert_true(rc == 0, "fs write error\n");

	rc = conf_test_file_strstr(cf.cf_name, "myfoo/mybar=8\n");
	zassert_true(rc == 0, "bad value read\n");

	val8 = 43;
	rc = conf_save();
	zassert_true(rc == 0, "fs write error\n");

	rc = conf_test_file_strstr(cf.cf_name, "myfoo/mybar=43\n");
	zassert_true(rc == 0, "bad value read\n");
}
