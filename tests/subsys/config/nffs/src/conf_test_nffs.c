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


u8_t val8;
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

struct conf_handler c_test_handlers[] = {
	{
		.ch_name = "myfoo",
		.ch_get = c1_handle_get,
		.ch_set = c1_handle_set,
		.ch_commit = c1_handle_commit,
		.ch_export = c1_handle_export
	},
};



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

int fsutil_read_file(const char *path, off_t offset, size_t len, void *dst,
		     size_t *out_len)
{
	struct fs_file_t file;
	int rc;
	ssize_t r_len = 0;

	rc = fs_open(&file, path);
	if (rc != 0) {
		return rc;
	}

	r_len = fs_read(&file, dst, len);
	if (r_len < 0) {
		rc = -EIO;
	} else {
		*out_len = r_len;
	}

	fs_close(&file);
	return rc;
}

int fsutil_write_file(const char *path, const void *data, size_t len)
{
	struct fs_file_t file;
	int rc;

	rc = fs_open(&file, path);
	if (rc != 0) {
		return rc;
	}

	if (fs_write(&file, data, len) != len) {
		rc = -EIO;
	}

	fs_close(&file);
	return rc;
}

int conf_test_file_strstr(const char *fname, char *string)
{
	int rc;
	u32_t len;
	u32_t rlen;
	char *buf;
	struct fs_dirent entry;

	rc = fs_stat(fname, &entry);
	if (rc) {
		return rc;
	}

	len = entry.size;
	buf = (char *)k_malloc(len + 1);
	zassert_not_null(buf, "out of memory\n");

	rc = fsutil_read_file(fname, 0, len, buf, &rlen);
	zassert_true(rc == 0, "can't access the file\n'");
	zassert_true(rc == 0, "not enough data read\n'");
	buf[rlen] = '\0';

	if (strstr(buf, string)) {
		return 0;
	}

	return -1;
}

void config_empty_lookups(void);
void config_test_insert(void);
void config_test_getset_unknown(void);
void config_test_getset_int(void);
void config_test_getset_bytes(void);
void config_test_getset_int64(void);
void config_test_commit(void);

void config_setup_nffs(void);
void config_test_empty_file(void);
void config_test_small_file(void);
void config_test_multiple_in_file(void);
void config_test_save_in_file(void);
void config_test_save_one_file(void);
void config_test_compress_file(void);

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
			 /* NFFS as backing storage. */
			 ztest_unit_test(config_setup_nffs),
			 ztest_unit_test(config_test_empty_file),
			 ztest_unit_test(config_test_small_file),
			 ztest_unit_test(config_test_multiple_in_file),
			 ztest_unit_test(config_test_save_in_file),
			 ztest_unit_test(config_test_save_one_file),
			 ztest_unit_test(config_test_compress_file)
			);

	ztest_run_test_suite(test_config_fcb);
}
