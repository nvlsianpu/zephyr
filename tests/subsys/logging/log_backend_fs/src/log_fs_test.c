/*
 * Copyright (c) 2020 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Test logging to file system
 *
 */

#include <stdbool.h>
#include <stdlib.h>
#include <zephyr.h>
#include <ztest.h>
#include <logging/log_backend.h>
#include <logging/log_ctrl.h>
#include <logging/log.h>
#include <fs/fs.h>
#if !CONFIG_LOG_BACKEND_FS_AUTOSTART
#include <fs/littlefs.h>
#endif

#define LOG_MODULE_NAME test
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define MAX_PATH_LEN 256
#define BACKEND_NAME STRINGIFY(log_backend_fs)
#define CORRECT_TEXT ("CORRECT "__TIME__ "\n")
#define WRONG_TEXT ("WRONG\n")

#define CORRECT_STR_LEN (sizeof(CORRECT_TEXT) - 1)
#define NO_B4_OVFL (CONFIG_LOG_BACKEND_FS_FILE_SIZE / CORRECT_STR_LEN)

#if !CONFIG_LOG_BACKEND_FS_AUTOSTART
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = "/lfs",
};
static struct log_backend *backend_fs_p;
#endif

static struct log_msg_ids src_level = {
	.level = LOG_LEVEL_INTERNAL_RAW_STRING,
	.source_id = 0, /* not used as level indicates raw string. */
	.domain_id = 0, /* not used as level indicates raw string. */
};

static const char *log_prefix = CONFIG_LOG_BACKEND_FS_FILE_PREFIX;
static char fname[MAX_PATH_LEN];

static void test_log_fs_setup(void)
{
#if !CONFIG_LOG_BACKEND_FS_AUTOSTART
	int rc;

	for (int i = 0; i < log_backend_count_get(); i++) {
		const struct log_backend *bp = log_backend_get(i);

		if (strncmp(BACKEND_NAME, bp->name,
			    sizeof(BACKEND_NAME)) == 0) {
			backend_fs_p = (struct log_backend *)bp;
			break;
		}
	}
	zassert_not_null(backend_fs_p, "LOG FS backend not found.");
	struct fs_mount_t *mp = &lfs_storage_mnt;

	rc = fs_mount(mp);
	zassert_equal(rc, 0, "Cannot mount file system.");
	log_backend_enable(backend_fs_p, NULL, CONFIG_LOG_MAX_LEVEL);
	backend_fs_p->api->init();
#endif

	log_0(CORRECT_TEXT, src_level);
	k_msleep(100);
}

static void test_log_fs_file_content(void)
{
	int rc;
	struct fs_dir_t dir = { 0 };
	struct fs_file_t file;
	char log_read[MAX_PATH_LEN];

	zassert_equal(fs_opendir(&dir, CONFIG_LOG_BACKEND_FS_MOUNT_POINT),
		      0, "Cannot open directory.");

	/* Iterate over mount point directory. */
	while (rc >= 0) {
		struct fs_dirent ent = { 0 };

		rc = fs_readdir(&dir, &ent);
		if ((rc < 0) || (ent.name[0] == 0)) {
			break;
		}
		if (strstr(ent.name, log_prefix) != NULL) {
			sprintf(fname, "%s/%s",
				CONFIG_LOG_BACKEND_FS_MOUNT_POINT,
				ent.name);
		}
	}
	(void)fs_closedir(&dir);

	zassert_equal(rc, 0, "Cannot seek for the last file.");

	zassert_equal(fs_open(&file, fname, FS_O_READ), 0,
		      "Cannot open log file.");

	zassert_true(fs_read(&file, log_read, MAX_PATH_LEN) >= 0,
		     "Cannot read log file.");

	zassert_equal(fs_close(&file), 0, "Cannot close log file.");

	zassert_not_null(strstr(log_read, CORRECT_TEXT),
			 "Text inside log file is not correct.");

	/* Check if not false positive. */
	zassert_is_null(strstr(log_read, WRONG_TEXT),
			"Log message false positive.");
}

static void test_log_fs_file_size(void)
{
	int rc;
	struct fs_dir_t dir = { 0 };
	int file_ctr = 0;

	/* Fill in log file over size limit. */
	for (int i = 0; i < NO_B4_OVFL; i++) {
		log_0(CORRECT_TEXT, src_level);
		k_msleep(1000);
	}
	zassert_equal(fs_opendir(&dir, CONFIG_LOG_BACKEND_FS_MOUNT_POINT),
		      0, "Cannot seek for last file.");

	/* Count number of log files. */
	while (rc >= 0) {
		struct fs_dirent ent = { 0 };

		rc = fs_readdir(&dir, &ent);
		if ((rc < 0) || (ent.name[0] == 0)) {
			break;
		}
		if (strstr(ent.name, log_prefix) != NULL) {
			++file_ctr;
		}
	}
	(void)fs_closedir(&dir);
	zassert_equal(file_ctr, 2, "File changing failed");
}

/* Test case main entry. */
void test_main(void)
{
	ztest_test_suite(test_log_backend_fs,
			 ztest_unit_test(test_log_fs_setup),
			 ztest_unit_test(test_log_fs_file_content),
			 ztest_unit_test(test_log_fs_file_size));
	ztest_run_test_suite(test_log_backend_fs);
}
