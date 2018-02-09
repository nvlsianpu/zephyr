/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"

void config_test_getset_int(void)
{
	char name[80];
	char tmp[64], *str;
	int rc;

	strcpy(name, "myfoo/mybar");
	rc = conf_set_value(name, "42");
	zassert_true(rc == 0, "can not set key value\n");
	zassert_true(test_set_called == 1, "the SET handler wasn't called\n");
	zassert_true(val8 == 42,
		     "SET handler: was called with wrong parameters\n");
	ctest_clear_call_state();

	strcpy(name, "myfoo/mybar");
	str = conf_get_value(name, tmp, sizeof(tmp));
	zassert_not_null(str, "the key value should been available\n");
	zassert_true(test_get_called == 1, "the GET handler wasn't called\n");
	zassert_true(!strcmp("42", tmp), "unexpected value fetched\n");
	ctest_clear_call_state();
}
