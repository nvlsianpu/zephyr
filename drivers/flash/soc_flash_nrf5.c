/*
 * Copyright (c) 2016 Linaro Limited
 *               2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <flash.h>
#include <string.h>

static inline bool is_aligned_32(u32_t data)
{
	return (data & 0x3) ? false : true;
}

static inline bool is_addr_valid(off_t addr, size_t len)
{
	if (addr + len > NRF_FICR->CODEPAGESIZE * NRF_FICR->CODESIZE ||
			addr < 0) {
		return false;
	}

	return true;
}

static void nvmc_wait_ready(void)
{
	while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
		;
	}
}

static int flash_nrf5_read(struct device *dev, off_t addr,
			    void *data, size_t len)
{
	if (!is_addr_valid(addr, len)) {
		return -EINVAL;
	}

	if (!len) {
		return 0;
	}

	memcpy(data, (void *)addr, len);

	return 0;
}

static int flash_nrf5_write(struct device *dev, off_t addr,
			     const void *data, size_t len)
{
	u32_t addr_word;
	u32_t tmp_word;
	void *data_word;
	u32_t remaining = len;
	u32_t count = 0;

	if (!is_addr_valid(addr, len)) {
		return -EINVAL;
	}

	if (!len) {
		return 0;
	}

	/* Start with a word-aligned address and handle the offset */
	addr_word = addr & ~0x3;

	/* If not aligned, read first word, update and write it back */
	if (!is_aligned_32(addr)) {
		tmp_word = *(u32_t *)(addr_word);
		count = sizeof(u32_t) - (addr & 0x3);
		if (count > len) {
			count = len;
		}
		memcpy((u8_t *)&tmp_word + (addr & 0x3), data, count);
		nvmc_wait_ready();
		*(u32_t *)addr_word = tmp_word;
		addr_word = addr + count;
		remaining -= count;
	}

	/* Write all the 4-byte aligned data */
	data_word = (void *) data + count;
	while (remaining >= sizeof(u32_t)) {
		nvmc_wait_ready();
		*(u32_t *)addr_word = *(u32_t *)data_word;
		addr_word += sizeof(u32_t);
		data_word += sizeof(u32_t);
		remaining -= sizeof(u32_t);
	}

	/* Write remaining data */
	if (remaining) {
		tmp_word = *(u32_t *)(addr_word);
		memcpy((u8_t *)&tmp_word, data_word, remaining);
		nvmc_wait_ready();
		*(u32_t *)addr_word = tmp_word;
	}

	nvmc_wait_ready();

	return 0;
}

static int flash_nrf5_erase(struct device *dev, off_t addr, size_t size)
{
	u32_t pg_size = NRF_FICR->CODEPAGESIZE;
	u32_t n_pages = size / pg_size;

	/* Erase can only be done per page */
	if (((addr % pg_size) != 0) || ((size % pg_size) != 0)) {
		return -EINVAL;
	}

	if (!is_addr_valid(addr, size)) {
		return -EINVAL;
	}

	if (!n_pages) {
		return 0;
	}

	/* Erase uses a specific configuration register */
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();

	for (u32_t i = 0; i < n_pages; i++) {
		NRF_NVMC->ERASEPAGE = (u32_t)addr + (i * pg_size);
		nvmc_wait_ready();
	}

	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();

	return 0;
}

static int flash_nrf5_write_protection(struct device *dev, bool enable)
{
	if (enable) {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
	} else {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
	}
	nvmc_wait_ready();

	return 0;
}

static const struct flash_driver_api flash_nrf5_api = {
	.read = flash_nrf5_read,
	.write = flash_nrf5_write,
	.erase = flash_nrf5_erase,
	.write_protection = flash_nrf5_write_protection,
};

// semaphore for synchronization flash opperations.
static struct k_sem flash_nrf5_sem;

static int nrf5_flash_init(struct device *dev)
{
	dev->driver_api = &flash_nrf5_api;

	k_sem_init(&flash_nrf5_sem, 0, 1);
	printk("flash init\n");

	return 0;
}

DEVICE_INIT(nrf5_flash, CONFIG_SOC_FLASH_NRF5_DEV_NAME, nrf5_flash_init,
	     NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);


#include "../../../subsys/bluetooth/controller/ticker/ticker.h"
#include "../../../subsys/bluetooth/controller/hal/radio.h"

#define FLASH_INTERVAL 1000000UL /* 1 sec */
#define FLASH_SLOT     100 /* 100 us */

extern u8_t ll_flash_ticker_id_get(); // from ll.c
extern void radio_state_abort(void); /* BLE controller abort intf. */

typedef void (*flash_op_handler_t) (void* context);

typedef struct {
	flash_op_handler_t p_op_handler;
	void * p_op_context; // [in,out]
} flash_op_desc_t;





static void time_slot_callback_work(u32_t ticks_at_expire, u32_t remainder, u16_t lazy,
		void *context)
{
	if (radio_is_idle()) {
		((flash_op_desc_t*)context)->p_op_handler(((flash_op_desc_t*)context)->p_op_context);
	} else {
		printk("Error: radio not idle!\n");
	}

	int err = ticker_stop(0, 0, 0, NULL, NULL);

	if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
			printk("Failed to stop ticker %d.\n",err);
	}

	/* notify thread that data is available */
	k_sem_give(&flash_nrf5_sem);
}

static void time_slot_callback_helper(u32_t ticks_at_expire, u32_t remainder, u16_t lazy,
		void *context)
{
		u32_t ticks_now;
		int err;

		u8_t ticker_id = ll_flash_ticker_id_get();

		ticks_now = ticker_ticks_now_get();

		radio_state_abort();

		/* start a secondary ticker after ~ 500 us, this will let any
		 * radio role to gracefully release the Radio h/w */

		err = ticker_start(0, /* Radio instance (can use define from ctrl.h) */
		   	0, /* user id for thread mode (MAYFLY_CALLER_ID_*) */
		   	0, /* flash ticker id */
		   	ticks_now/*ticks_at_expire*/, /* current tick */
			TICKER_US_TO_TICKS(500), /* first int. */
		   	TICKER_US_TO_TICKS(FLASH_INTERVAL), /* periodic */
		   	TICKER_REMAINDER(FLASH_INTERVAL), /* per. remaind.*/
		   	0, /* lazy, voluntary skips */
		   	TICKER_US_TO_TICKS(FLASH_SLOT),
			time_slot_callback_work,
			context,
		   	NULL, /* no op callback */
		   	NULL);

		if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
				printk("Failed to start 2nd ticker %d.\n",err);
		}

		err = ticker_stop(0, 3, ticker_id, NULL, NULL);

		if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
				printk("Failed to stop ticker %d.\n",err);
		}
}


int work_in_time_slot(flash_op_desc_t * p_flash_op_desc)
{


	u8_t ticker_id = ll_flash_ticker_id_get();
	u32_t err;

	/*
	u32_t ticker_start(u8_t instance_index, u8_t user_id, u8_t ticker_id,
		   u32_t ticks_anchor, u32_t ticks_first, u32_t ticks_periodic,
		   u32_t remainder_periodic, u16_t lazy, u16_t ticks_slot,
		   ticker_timeout_func ticker_timeout_func, void *context,
		   ticker_op_func fp_op_func, void *op_context);
	 */

	err = ticker_start(0, /* Radio instance (can use define from ctrl.h) */
			   3, /* user id for thread mode (MAYFLY_CALLER_ID_*) */
			   ticker_id, /* flash ticker id */
			   ticker_ticks_now_get(), /* current tick */
			   TICKER_US_TO_TICKS(FLASH_INTERVAL/100), /* first int. */
			   TICKER_US_TO_TICKS(FLASH_INTERVAL), /* periodic */
			   TICKER_REMAINDER(FLASH_INTERVAL), /* per. remaind.*/
			   0, /* lazy, voluntary skips */
			   TICKER_US_TO_TICKS(FLASH_SLOT),
			   time_slot_callback_helper,
			   p_flash_op_desc,
			   NULL, /* no op callback */
			   NULL);

	if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
		printk("Failed to start ticker 1.\n");
		return err;
	}

	if (k_sem_take(&flash_nrf5_sem, K_MSEC(200)) != 0) {
	        printk("flash_nrf5_sem not available!\n");
	        err = 4;
	}

	return err;
}

