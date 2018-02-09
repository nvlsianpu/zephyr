/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvconf_test.h"
#include <fs.h>

void config_setup_nffs(void)
{
	int rc;

	rc = fs_unlink(TEST_CONFIG_DIR);
	zassert_true(rc == 0 || rc == -ENOENT,
		      "can't delete config directory%d\n", rc);
}
