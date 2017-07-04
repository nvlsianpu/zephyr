/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2016-2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_DOMAIN "fota/mcuboot"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_FOTA_LEVEL
#include <logging/sys_log.h>

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <flash.h>
#include <zephyr.h>
#include <init.h>

#include "boot/mcuboot.h"

/*
 * Helpers for image trailer, as defined by mcuboot.
 * Image trailer consists of sequence of fields:
 *   u8_t copy_done
 *   u8_t pading_1[BOOT_MAX_ALIGN - 1]
 *   u8_t image_ok
 *   u8_t pading_2[BOOT_MAX_ALIGN - 1]
 *   u8_t magic[16]
 */

/* Strict defines: Defines in block below must be equal to coresponding
 * mcuboot defines */
#define BOOT_MAX_ALIGN 8
#define BOOT_MAGIC_SZ  16
#define BOOT_FLAG_SET 0x01
#define BOOT_FLAG_UNSET 0xff
/* end_of Strict defines */

#define BOOT_MAGIC_GOOD  1
#define BOOT_MAGIC_BAD   2
#define BOOT_MAGIC_UNSET 3

#define BOOT_FLAG_IMAGE_OK 0
#define BOOT_FLAG_COPY_DONE 1

#define COPY_DONE_OFFS(bank_offs) (bank_offs + FLASH_BANK_SIZE -\
				   BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 2)

#define IMAGE_OK_OFFS(bank_offs) (bank_offs + FLASH_BANK_SIZE - BOOT_MAGIC_SZ -\
				  BOOT_MAX_ALIGN)
#define MAGIC_OFFS(bank_offs) (bank_offs + FLASH_BANK_SIZE - BOOT_MAGIC_SZ)

const u32_t boot_img_magic[4] = {
	0xf395c277,
	0x7fefd260,
	0x0f505235,
	0x8079b62c,
};

static struct device *flash_dev;

static int boot_flag_offs(int flag, u32_t bank_offs, u32_t *offs)
{
	switch (flag) {
	case BOOT_FLAG_COPY_DONE:
		*offs = COPY_DONE_OFFS(bank_offs);
		return 0;
	case BOOT_FLAG_IMAGE_OK:
		*offs = IMAGE_OK_OFFS(bank_offs);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int boot_write_flag(int flag, u32_t bank_offs)
{
	u8_t buf[FLASH_MIN_WRITE_SIZE];
	u32_t offs;
	int rc;

	rc = boot_flag_offs(flag, bank_offs, &offs);
	if (rc != 0) {
		return rc;
	}

	memset(buf, 0xFF, sizeof(buf));
	buf[0] = BOOT_FLAG_SET;

	flash_write_protection_set(flash_dev, false);
	rc = flash_write(flash_dev, offs, buf, sizeof(buf));
	flash_write_protection_set(flash_dev, true);

	return rc;
}

static int boot_flag_get(int flag, u32_t bank_offs)
{
	u32_t offs;
	int rc;
	u8_t flag_val;

	rc = boot_flag_offs(flag, bank_offs, &offs);
	if (rc != 0) {
		return rc;
	}

	rc = flash_read(flash_dev, offs, &flag_val, sizeof(flag_val));
	if (rc != 0) {
		return rc;
	}

	return flag_val;
}

static u8_t boot_copy_done_get(u32_t bank_offs)
{
	return boot_flag_get(BOOT_FLAG_COPY_DONE, bank_offs);
}

static int boot_image_ok_get(u32_t bank_offs)
{
	return boot_flag_get(BOOT_FLAG_IMAGE_OK, bank_offs);
}

static int boot_write_image_ok(u32_t bank_offs)
{
	return boot_write_flag(BOOT_FLAG_IMAGE_OK, bank_offs);
}

static int boot_write_magic(u32_t bank_offs)
{
	u32_t offs;
	int rc;

	offs = MAGIC_OFFS(bank_offs);

	flash_write_protection_set(flash_dev, false);
	rc = flash_write(flash_dev, offs, boot_img_magic, BOOT_MAGIC_SZ);
	flash_write_protection_set(flash_dev, true);

	return rc;
}

static int boot_magic_code_check(const u32_t *magic)
{
	int i;

	if (memcmp(magic, boot_img_magic, sizeof(boot_img_magic)) == 0) {
		return BOOT_MAGIC_GOOD;
	}

	for (i = 0; i < 4; i++) {
		if (magic[i] == 0xffffffff) {
			return BOOT_MAGIC_UNSET;
		}
	}

	return BOOT_MAGIC_BAD;
}

static int boot_magic_state_get(u32_t bank_offs)
{
	u32_t magic[4];
	u32_t offs;
	int rc;

	offs = MAGIC_OFFS(bank_offs);
	rc = flash_read(flash_dev, offs, magic, sizeof(magic));
	if (rc != 0) {
		return rc;
	}

	return boot_magic_code_check(magic);
}

int boot_img_pending_set(int permanent)
{
	int rc;

	switch (boot_magic_state_get(FLASH_BANK1_OFFSET)) {
	case BOOT_MAGIC_GOOD:
		/* Swap already scheduled. */
		return 0;

	case BOOT_MAGIC_UNSET:
		rc = boot_write_magic(FLASH_BANK1_OFFSET);
		if (rc == 0 && permanent) {
			rc = boot_write_image_ok(FLASH_BANK1_OFFSET);
		}

		return rc;

	default:
		return -EFAULT;
	}
}

int boot_img_confirmed_set(void)
{
	int rc;

	switch (boot_magic_state_get(FLASH_BANK0_OFFSET)) {
	case BOOT_MAGIC_GOOD:
		/* Confirm needed; proceed. */
		break;

	case BOOT_MAGIC_UNSET:
		/* Already confirmed. */
		return 0;

	case BOOT_MAGIC_BAD:
		/* Unexpected state. */
		return -EFAULT;
	}

	if (boot_copy_done_get(FLASH_BANK0_OFFSET) == BOOT_FLAG_UNSET) {
		/* Swap never completed.  This is unexpected. */
		return -EFAULT;
	}

	if (boot_image_ok_get(FLASH_BANK0_OFFSET) != BOOT_FLAG_UNSET) {
		/* Already confirmed. */
		return 0;
	}

	rc = boot_write_image_ok(FLASH_BANK0_OFFSET);

	return rc;
}

int boot_img_bank_erase(u32_t bank_offset)
{
	int rc;

	flash_write_protection_set(flash_dev, false);
	rc = flash_erase(flash_dev, bank_offset, FLASH_BANK_SIZE);
	flash_write_protection_set(flash_dev, true);

	return rc;
}

static int boot_init(struct device *dev)
{
	ARG_UNUSED(dev);
	flash_dev = device_get_binding(FLASH_DRIVER_NAME);
	if (!flash_dev) {
		SYS_LOG_ERR("Failed to find the flash driver");
		return -ENODEV;
	}
	return 0;
}

SYS_INIT(boot_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
