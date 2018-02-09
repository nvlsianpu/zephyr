/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include "config/config_file.h"

void config_test_save_one_file(void)
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

	val8 = 33;
	rc = conf_save();
	zassert_true(rc == 0, "fs write error\n");

	rc = conf_save_one("myfoo/mybar", "42");
	zassert_equal(rc, 0, "fs one item write error\n");

	rc = conf_load();
	zassert_true(rc == 0, "fs redout error\n");
	zassert_true(val8 == 42, "bad value read\n");

	rc = conf_save_one("myfoo/mybar", "44");
	zassert_true(rc == 0, "fs one item write error\n");

	rc = conf_load();
	zassert_true(rc == 0, "fs redout error\n");
	zassert_true(val8 == 44, "bad value read\n");
}
