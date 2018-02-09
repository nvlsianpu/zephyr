/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CONF_TEST_FCB_H
#define _CONF_TEST_FCB_H

#include <stdio.h>
#include <string.h>
#include <ztest.h>

#include "config/config.h"
#include "flash_map.h"

#ifdef __cplusplus
#extern "C" {
#endif

#define CONF_TEST_FCB_VAL_STR_CNT   64
#define CONF_TEST_FCB_FLASH_CNT   4

extern u8_t val8;
extern u32_t val32;
extern u64_t val64;

extern int test_get_called;
extern int test_set_called;
extern int test_commit_called;
extern int test_export_block;

extern int c2_var_count;

extern struct flash_sector fcb_sectors[CONF_TEST_FCB_FLASH_CNT];

extern char val_string[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];
extern char test_ref_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];

extern struct conf_handler c_test_handlers[];

void ctest_clear_call_state(void);
int ctest_get_call_state(void);

void config_wipe_srcs(void);
void config_wipe_fcb(struct flash_sector *fs, int cnt);

void config_test_fill_area(
	char test_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN],
		int iteration);

#ifdef __cplusplus
}
#endif
#endif /* _CONF_TEST_FCB_H */
