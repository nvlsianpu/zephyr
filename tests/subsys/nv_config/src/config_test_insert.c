/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"

void config_test_insert_x(int idx)
{
	int rc;

	rc = conf_register(&c_test_handlers[idx]);
	zassert_true(rc == 0, "conf_register fail");
}

void config_test_insert(void)
{
	config_test_insert_x(0);
}

void config_test_insert2(void)
{
	config_test_insert_x(1);
}

void config_test_insert3(void)
{
	config_test_insert_x(2);
}
