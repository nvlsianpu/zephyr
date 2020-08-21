/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <logging/log_backend.h>
#include "log_backend_std.h"
#include <assert.h>
#include <fs/fs.h>
#include <fs/littlefs.h>

#define MAX_PATH_LEN 256
#define MAX_FLASH_WRITE_SIZE 256
#define LOG_PREFIX_LEN (sizeof(CONFIG_LOG_BACKEND_FS_FILE_PREFIX) - 1)

enum backend_fs_state {
	BACKEND_FS_NOT_INITIALIZED = 0,
	BACKEND_FS_CORRUPTED,
	BACKEND_FS_OK
};

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);

static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = CONFIG_LOG_BACKEND_FS_MOUNT_POINT,
};

static char fname[MAX_PATH_LEN];
static struct fs_file_t file;
static enum backend_fs_state backend_state = BACKEND_FS_NOT_INITIALIZED;
static int file_ctr;

static int allocate_new_file(struct fs_file_t *file);
static int del_oldest_log(void);

static int write_log_to_file(uint8_t *data, size_t length, void *ctx)
{
	if (backend_state == BACKEND_FS_OK) {
		int rc;
		struct fs_file_t *f = &file;

		/* Check if new data overwrites max file size.
		 * If so, create new log file.
		 */
		int size = fs_tell(f);

		if (size < 0) {
			backend_state = BACKEND_FS_CORRUPTED;

			return length;
		} else if ((size + length) > CONFIG_LOG_BACKEND_FS_FILE_SIZE) {
			rc = allocate_new_file(f);

			if (rc < 0) {
				goto on_error;
			}
		}

		rc = fs_write(f, data, length);
		if (rc == -ENOSPC) {
			if (IS_ENABLED(CONFIG_LOG_BACKEND_FS_OVERWRITE)) {
				del_oldest_log();

				return 0;
			}
			/* If overwrite is disabled, full memory
			 * is equivalent of corrupted backend.
			 */
			goto on_error;
		} else if (rc < 0) {
			return 0;
		}

		fs_sync(f);
	}

	return length;

on_error:
	backend_state = BACKEND_FS_CORRUPTED;
	return length;
}

static uint8_t __aligned(4) buf[MAX_FLASH_WRITE_SIZE];
LOG_OUTPUT_DEFINE(log_output, write_log_to_file, buf, MAX_FLASH_WRITE_SIZE);

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_backend_std_put(&log_output, 0, msg);
}

static void log_backend_fs_init(void)
{
	int rc;
	if (IS_ENABLED(CONFIG_LOG_BACKEND_FS_AUTOSTART)) {
		/* If autostart, mount file system automatically. */
		struct fs_mount_t *mp = &lfs_storage_mnt;

		rc = fs_mount(mp);
		if (rc < 0) {
			backend_state = BACKEND_FS_CORRUPTED;

			return;
		}
	}

	rc = allocate_new_file(&file);
	backend_state = (rc ? BACKEND_FS_CORRUPTED : BACKEND_FS_OK);
}

static void panic(struct log_backend const *const backend)
{
	/* In case of panic deinitialize backend. It is better to keep
	 * current data rather than log new and risk of failure.
	 */
	log_backend_deactivate(backend);
}

static void dropped(const struct log_backend *const backend, uint32_t cnt)
{
	ARG_UNUSED(backend);

	log_backend_std_dropped(&log_output, cnt);
}

static int allocate_new_file(struct fs_file_t *file)
{
	/* In case of no log file or current file fills up
	 * create new log file.
	 */
	int rc;
	int file_num = 0;
	struct fs_statvfs stat;

	assert(file);

	if (backend_state == BACKEND_FS_NOT_INITIALIZED) {
		/* Search for the last used log number. */
		struct fs_dir_t dir = { 0 };

		rc = fs_opendir(&dir, CONFIG_LOG_BACKEND_FS_MOUNT_POINT);

		while (rc >= 0) {
			struct fs_dirent ent = { 0 };

			rc = fs_readdir(&dir, &ent);
			if ((rc < 0) || (ent.name[0] == 0)) {
				break;
			}
			if (strstr(ent.name, CONFIG_LOG_BACKEND_FS_FILE_PREFIX)
			    != NULL) {
				++file_ctr;
				file_num = atoi(ent.name + LOG_PREFIX_LEN) + 1;
			}
		}

		(void)fs_closedir(&dir);
		if (rc < 0) {
			goto out;
		}

		backend_state = BACKEND_FS_OK;
	} else {
		fs_close(file);
		char *name = strstr(fname, CONFIG_LOG_BACKEND_FS_FILE_PREFIX);

		file_num = atoi(name + LOG_PREFIX_LEN) + 1;
	}

	sprintf(fname, sizeof(del_file_path), "%s/%s%04d",
		CONFIG_LOG_BACKEND_FS_MOUNT_POINT,
		CONFIG_LOG_BACKEND_FS_FILE_PREFIX, file_num);
	rc = fs_statvfs(CONFIG_LOG_BACKEND_FS_MOUNT_POINT, &stat);
	if (rc < 0) {
		goto out;
	}

	/* Check if there is enough space to write file or max files number
	 * is not exceeded.
	 */
	while ((file_ctr >= CONFIG_LOG_BACKEND_FS_FILES_LIMIT) ||
	       ((stat.f_bfree * stat.f_frsize) <=
		CONFIG_LOG_BACKEND_FS_FILE_SIZE)) {

		if (IS_ENABLED(CONFIG_LOG_BACKEND_FS_OVERWRITE)) {

			rc = del_oldest_log();
			if ((rc < 0) && (rc != -ENOENT)) {
				goto out;
			}
			rc = fs_statvfs(CONFIG_LOG_BACKEND_FS_MOUNT_POINT,
					&stat);
			if (rc < 0) {
				goto out;
			}

		} else {
			return -ENOSPC;
		}
	}

	rc = fs_open(file, fname, FS_O_CREATE | FS_O_WRITE);
	if (rc < 0) {
		goto out;
	}
	++file_ctr;

out:
	return rc;
}

static int del_oldest_log(void)
{
	struct fs_dir_t dir = { 0 };
	int rc;
	char del_file_path[MAX_FILE_NAME] = "\0";

	/* Look for a first file with log prefix. */
	rc = fs_opendir(&dir, CONFIG_LOG_BACKEND_FS_MOUNT_POINT);
	while (rc >= 0) {
		struct fs_dirent ent = { 0 };

		rc = fs_readdir(&dir, &ent);
		if (rc < 0) {
			break;
		}
		if (strstr(ent.name, CONFIG_LOG_BACKEND_FS_FILE_PREFIX) !=
		    NULL) {
			sprintf(del_file_path, sizeof(del_file_path), "%s/%s",
				CONFIG_LOG_BACKEND_FS_MOUNT_POINT, ent.name);
			break;
		}
	}
	if (del_file_path[0] != '\0') {
		rc = fs_unlink(del_file_path);
		if (rc == 0) {
			--file_ctr;
		}
	}

	fs_closedir(&dir);

	return rc;
}

BUILD_ASSERT(!IS_ENABLED(CONFIG_LOG_IMMEDIATE),\
	"Immediate logging is not supported by LOG FS backend.")
static const struct log_backend_api log_backend_fs_api = {
	.put = put,
	.put_sync_string = NULL,
	.put_sync_hexdump = NULL,
	.panic = panic,
	.init = log_backend_fs_init,
	.dropped = dropped,
};

LOG_BACKEND_DEFINE(log_backend_fs, log_backend_fs_api,
		   IS_ENABLED(CONFIG_LOG_BACKEND_FS_AUTOSTART) ?
		   true : false);
