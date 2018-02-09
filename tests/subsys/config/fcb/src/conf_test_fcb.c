/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>

#include "nvconf_test.h"
#include "config_priv.h"
#include "flash_map.h"

u8_t val8;
u32_t val32;
u64_t val64;

int test_get_called;
int test_set_called;
int test_commit_called;
int test_export_block;

int c2_var_count = 1;

char *c1_handle_get(int argc, char **argv, char *val, int val_len_max);
int c1_handle_set(int argc, char **argv, char *val);
int c1_handle_commit(void);
int c1_handle_export(void (*cb)(char *name, char *value),
			enum conf_export_tgt tgt);

char *c2_handle_get(int argc, char **argv, char *val, int val_len_max);
int c2_handle_set(int argc, char **argv, char *val);
int c2_handle_export(void (*cb)(char *name, char *value),
		     enum conf_export_tgt tgt);

char *c3_handle_get(int argc, char **argv, char *val, int val_len_max);
int c3_handle_set(int argc, char **argv, char *val);
int c3_handle_export(void (*cb)(char *name, char *value),
		     enum conf_export_tgt tgt);

struct conf_handler c_test_handlers[] = {
	{
		.ch_name = "myfoo",
		.ch_get = c1_handle_get,
		.ch_set = c1_handle_set,
		.ch_commit = c1_handle_commit,
		.ch_export = c1_handle_export
	},
	{
		.ch_name = "2nd",
		.ch_get = c2_handle_get,
		.ch_set = c2_handle_set,
		.ch_commit = NULL,
		.ch_export = c2_handle_export
	},
	{
		.ch_name = "3",
		.ch_get = c3_handle_get,
		.ch_set = c3_handle_set,
		.ch_commit = NULL,
		.ch_export = c3_handle_export
	}
};

char val_string[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];
char test_ref_value[CONF_TEST_FCB_VAL_STR_CNT][CONF_MAX_VAL_LEN];

char *c1_handle_get(int argc, char **argv, char *val, int val_len_max)
{
	test_get_called = 1;

	if (argc == 1 && !strcmp(argv[0], "mybar")) {
		return conf_str_from_value(CONF_INT8, &val8, val, val_len_max);
	}

	if (argc == 1 && !strcmp(argv[0], "mybar64")) {
		return conf_str_from_value(CONF_INT64, &val64, val,
					   val_len_max);
	}

	return NULL;
}

int c1_handle_set(int argc, char **argv, char *val)
{
	u8_t newval;
	u64_t newval64;
	int rc;

	test_set_called = 1;
	if (argc == 1 && !strcmp(argv[0], "mybar")) {
		rc = CONF_VALUE_SET(val, CONF_INT8, newval);
		zassert_true(rc == 0, "CONF_VALUE_SET callback");
		val8 = newval;
		return 0;
	}

	if (argc == 1 && !strcmp(argv[0], "mybar64"))	 {
		rc = CONF_VALUE_SET(val, CONF_INT64, newval64);
		zassert_true(rc == 0, "CONF_VALUE_SET callback");
		val64 = newval64;
		return 0;
	}

	return -ENOENT;
}

int c1_handle_commit(void)
{
	test_commit_called = 1;
	return 0;
}

int c1_handle_export(void (*cb)(char *name, char *value),
			enum conf_export_tgt tgt)
{
	char value[32];

	if (test_export_block) {
		return 0;
	}

	conf_str_from_value(CONF_INT8, &val8, value, sizeof(value));
	cb("myfoo/mybar", value);

	conf_str_from_value(CONF_INT64, &val64, value, sizeof(value));
	cb("myfoo/mybar64", value);

	return 0;
}

void ctest_clear_call_state(void)
{
	test_get_called = 0;
	test_set_called = 0;
	test_commit_called = 0;
}

int ctest_get_call_state(void)
{
	return test_get_called + test_set_called + test_commit_called;
}

void config_wipe_srcs(void)
{
	sys_slist_init(&conf_load_srcs);
	conf_save_dst = NULL;
}

struct flash_sector fcb_sectors[CONF_TEST_FCB_FLASH_CNT] = {
	[0] = {
		.fs_off = 0x00000000,
		.fs_size = 16 * 1024
	},
	[1] = {
		.fs_off = 0x00004000,
		.fs_size = 16 * 1024
	},
	[2] = {
		.fs_off = 0x00008000,
		.fs_size = 16 * 1024
	},
	[3] = {
		.fs_off = 0x0000c000,
		.fs_size = 16 * 1024
	}
};

void config_wipe_fcb(struct flash_sector *fs, int cnt)
{
	const struct flash_area *fap;
	int rc;
	int i;

	rc = flash_area_open(CONFIG_CFGSYS_FCB_FLASH_AREA, &fap);

	for (i = 0; i < cnt; i++) {
		rc = flash_area_erase(fap, fs[i].fs_off, fs[i].fs_size);
		zassert_true(rc == 0, "Can't get flash area\n");
	}
}

void
config_test_fill_area(char test_value[CONF_TEST_FCB_VAL_STR_CNT]
				     [CONF_MAX_VAL_LEN],
		      int iteration)
{
	int i, j;

	for (j = 0; j < 64; j++) {
		for (i = 0; i < CONF_MAX_VAL_LEN; i++) {
			test_value[j][i] = ((j * 2) + i + iteration) % 10 + '0';
		}
		test_value[j][sizeof(test_value[j]) - 1] = '\0';
	}
}

char *c2_var_find(char *name)
{
	int idx = 0;
	int len;
	char *eptr;

	len = strlen(name);
	zassert_true(len > 6, "string type expected\n");
	zassert_true(!strncmp(name, "string", 6), "string type expected\n");

	idx = strtoul(&name[6], &eptr, 10);
	zassert_true(*eptr == '\0', "EOF\n");
	zassert_true(idx < c2_var_count,
		     "var index greather than any exporter\n");

	return val_string[idx];
}

char *c2_handle_get(int argc, char **argv, char *val, int val_len_max)
{
	int len;
	char *valptr;

	if (argc == 1) {
		valptr = c2_var_find(argv[0]);
		if (!valptr) {
			return NULL;
		}

		len = strlen(val_string[0]);
		if (len > val_len_max) {
			len = val_len_max;
		}

		strncpy(val, valptr, len);
	}

	return NULL;
}

int c2_handle_set(int argc, char **argv, char *val)
{
	char *valptr;

	if (argc == 1) {
		valptr = c2_var_find(argv[0]);
		if (!valptr) {
			return -ENOENT;
		}

		if (val) {
			strncpy(valptr, val, sizeof(val_string[0]));
		} else {
			memset(valptr, 0, sizeof(val_string[0]));
		}

		return 0;
	}

	return -ENOENT;
}

int c2_handle_export(void (*cb)(char *name, char *value),
		     enum conf_export_tgt tgt)
{
	int i;
	char name[32];

	for (i = 0; i < c2_var_count; i++) {
		snprintf(name, sizeof(name), "2nd/string%d", i);
		cb(name, val_string[i]);
	}

	return 0;
}

char *c3_handle_get(int argc, char **argv, char *val, int val_len_max)
{
	if (argc == 1 && !strcmp(argv[0], "v")) {
		return conf_str_from_value(CONF_INT32, &val32, val,
					   val_len_max);
	}

	return NULL;
}

int c3_handle_set(int argc, char **argv, char *val)
{
	u32_t newval;
	int rc;

	if (argc == 1 && !strcmp(argv[0], "v")) {
		rc = CONF_VALUE_SET(val, CONF_INT32, newval);
		zassert_true(rc == 0, "CONF_VALUE_SET callback");
		val32 = newval;
		return 0;
	}

	return -ENOENT;
}

int c3_handle_export(void (*cb)(char *name, char *value),
		     enum conf_export_tgt tgt)
{
	char value[32];

	conf_str_from_value(CONF_INT32, &val32, value, sizeof(value));
	cb("3/v", value);

	return 0;
}

void config_empty_lookups(void);
void config_test_insert(void);
void config_test_getset_unknown(void);
void config_test_getset_int(void);
void config_test_getset_bytes(void);
void config_test_getset_int64(void);
void config_test_commit(void);

void config_test_empty_fcb(void);
void config_test_save_1_fcb(void);
void config_test_insert2(void);
void config_test_save_2_fcb(void);
void config_test_insert3(void);
void config_test_save_3_fcb(void);
void config_test_compress_reset(void);
void config_test_save_one_fcb(void);

void test_main(void *p1, void *p2, void *p3)
{
	ztest_test_suite(test_config_fcb,
			 /* Config tests */
			 ztest_unit_test(config_empty_lookups),
			 ztest_unit_test(config_test_insert),
			 ztest_unit_test(config_test_getset_unknown),
			 ztest_unit_test(config_test_getset_int),
			 ztest_unit_test(config_test_getset_bytes),
			 ztest_unit_test(config_test_getset_int64),
			 ztest_unit_test(config_test_commit),
			 /* FCB as backing storage*/
			 ztest_unit_test(config_test_empty_fcb),
			 ztest_unit_test(config_test_save_1_fcb),
			 ztest_unit_test(config_test_insert2),
			 ztest_unit_test(config_test_save_2_fcb),
			 ztest_unit_test(config_test_insert3),
			 ztest_unit_test(config_test_save_3_fcb),
			 ztest_unit_test(config_test_compress_reset),
			 ztest_unit_test(config_test_save_one_fcb)
			);

	ztest_run_test_suite(test_config_fcb);
}
