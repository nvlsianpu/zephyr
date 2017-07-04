/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FLASH_IMG_H__
#define __FLASH_IMG_H__

#ifdef __cplusplus
extern "C" {
#endif

struct flash_img_contex {
	u8_t buf[CONFIG_IMG_BLOCK_BUF_SIZE];
	struct device *dev;
	u32_t bytes_written;
	u16_t buf_bytes;
};

/**
 * @brief Initialize of context needed for proceed the image writing.
 *
 * @param cb context to be initialized
 * @param dev flash driver to be used to proceed the writing
 */
void img_writer_init(struct flash_img_contex *cb, struct device *dev);

/**
 * @brief Extract count of bytes written to the firmware image.
 *
 * @param cb context
 *
 * @retval Count of bytes written to the image flash.
 */
u32_t img_written_size_get(struct flash_img_contex *cb);

/**
 * @brief  Process input buffers to be written to the 1. image bank flash
 * memory in single blocks. Will store remainder between calls.
 *
 * A final call to this function with finished set to true
 * will write out the remaining block buffer to flash.
 *
 * @param cb context
 * @param data data to write
 * @param len Number of bytes to write
 * @param finished when true this forces any buffered
 * data to be written to flash
 *
 * @return  0 on success, negative errno code on fail
 */
int img_block_write(struct flash_img_contex *cb, u8_t *data,
		    int len, bool finished);

#ifdef __cplusplus
}
#endif

#endif	/* __FLASH_IMG_H__ */
