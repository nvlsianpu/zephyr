/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_DOMAIN "fota/flash_block"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_FOTA_LEVEL
#include <logging/sys_log.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <flash.h>
#include "boot/mcuboot_constraints.h"
#include "img_util/flash_block.h"

#if (CONFIG_IMG_BLOCK_BUF_SIZE % FLASH_ALIGN != 0)
#error CONFIG_IMG_BLOCK_BUF_SIZE is not multiple of FLASH_ALIGN
#endif

static bool flash_verify(struct device *dev, off_t offset,
			 u8_t *data, int len)
{
	u32_t temp;
	int size;
	int rc;

	while (len) {
		size = (len >= sizeof(temp)) ? sizeof(temp) : len;
		rc = flash_read(dev, offset, &temp, size);
		if (rc) {
			SYS_LOG_ERR("flash_read error %d addr=0x%08x",
				    rc, offset);
			break;
		}

		if (memcmp(data, &temp, size)) {
			SYS_LOG_ERR("addr=0x%08x VERIFY FAIL", offset);
			break;
		}
		len -= size;
		offset += size;
		data += size;
	}

	return (len == 0) ? true : false;
}

/* buffer data into block writes */
static int flash_block_write(struct flash_img_contex *cb, off_t offset,
			     u8_t *data, int len, bool finished)
{
	int processed = 0;
	int rc = 0;

	while ((len - processed) >
	       (CONFIG_IMG_BLOCK_BUF_SIZE - cb->buf_bytes)) {
		memcpy(cb->buf + cb->buf_bytes, data + processed,
		       (CONFIG_IMG_BLOCK_BUF_SIZE - cb->buf_bytes));

		flash_write_protection_set(cb->dev, false);
		rc = flash_write(cb->dev, offset + cb->bytes_written,
				 cb->buf, CONFIG_IMG_BLOCK_BUF_SIZE);
		flash_write_protection_set(cb->dev, true);
		if (rc) {
			SYS_LOG_ERR("flash_write error %d offset=0x%08x",
				    rc, offset + cb->bytes_written);
			return rc;
		}

		if (!rc &&
		    !flash_verify(cb->dev, offset + cb->bytes_written,
				  cb->buf, CONFIG_IMG_BLOCK_BUF_SIZE)) {
			return -EIO;
		}

		cb->bytes_written += CONFIG_IMG_BLOCK_BUF_SIZE;
		processed += (CONFIG_IMG_BLOCK_BUF_SIZE - cb->buf_bytes);
		cb->buf_bytes = 0;
	}

	/* place rest of the data into cb->buf */
	if (processed < len) {
		memcpy(cb->buf + cb->buf_bytes,
		       data + processed, len - processed);
		cb->buf_bytes += len - processed;
	}

	if (finished && cb->buf_bytes > 0) {
		/* pad the rest of cb->buf and write it out */
		memset(cb->buf + cb->buf_bytes, 0xFF,
		       CONFIG_IMG_BLOCK_BUF_SIZE - cb->buf_bytes);

		flash_write_protection_set(cb->dev, false);
		rc = flash_write(cb->dev, offset + cb->bytes_written,
				 cb->buf, CONFIG_IMG_BLOCK_BUF_SIZE);
		flash_write_protection_set(cb->dev, true);
		if (rc) {
			SYS_LOG_ERR("flash_write error %d offset=0x%08x",
				    rc, offset + cb->bytes_written);
			return rc;
		}

		if (!rc &&
		    !flash_verify(cb->dev, offset + cb->bytes_written,
				  cb->buf, CONFIG_IMG_BLOCK_BUF_SIZE)) {
			return -EIO;
		}

		cb->bytes_written = cb->bytes_written + cb->buf_bytes;
		cb->buf_bytes = 0;
	}

	return rc;
}

u32_t img_written_size_get(struct flash_img_contex *cb)
{
	return cb->bytes_written;
}

void img_writer_init(struct flash_img_contex *cb, struct device *dev)
{
	cb->dev = dev;
	cb->bytes_written = 0;
	cb->buf_bytes = 0;
}

int img_block_write(struct flash_img_contex *cb, u8_t *data, int len,
		    bool finished)
{
	return flash_block_write(cb, FLASH_BANK1_OFFSET, data, len, finished);
}
