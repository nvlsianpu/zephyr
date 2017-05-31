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

int write_in_timeslot(off_t addr, const void *data, size_t len);
int write_in_current_context(off_t addr, const void *data, size_t len);

#define IS_BLE_INITIALIZED() 1// @todo is_ble_initialized)


static int flash_nrf5_write(struct device *dev, off_t addr,
			     const void *data, size_t len)
{
	int ret;

	if (!is_addr_valid(addr, len)) {
		return -EINVAL;
	}

	if (!len) {
		return 0;
	}

	if ( IS_ENABLED(CONFIG_BLUETOOTH_CONTROLLER) && IS_BLE_INITIALIZED()) {
		ret = write_in_timeslot(addr, data, len);
	} else {
		ret = write_in_current_context(addr, data, len);
	}

	return ret;
}

/*static*/ int erase_in_timeslot(u32_t addr, u32_t size);
/*static*/ int erase_in_current_context(u32_t addr, u32_t size);

static int flash_nrf5_erase(struct device *dev, off_t addr, size_t size)
{
	u32_t pg_size = NRF_FICR->CODEPAGESIZE;
	u32_t n_pages = size / pg_size;
	int   ret;

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

	if ( IS_ENABLED(CONFIG_BLUETOOTH_CONTROLLER) && IS_BLE_INITIALIZED()) {
		ret = erase_in_timeslot(addr, size);
	} else {
		ret = erase_in_current_context(addr, size);
	}

	return ret;
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

// semaphore for lock flash resorces (tickers).
static struct k_sem flash_busy_nrf5_sem;

static int nrf5_flash_init(struct device *dev)
{
	dev->driver_api = &flash_nrf5_api;

	k_sem_init(&flash_nrf5_sem, 0, 1);
	k_sem_init(&flash_busy_nrf5_sem, 1, 1);
	printk("flash init\n");

	return 0;
}

DEVICE_INIT(nrf5_flash, CONFIG_SOC_FLASH_NRF5_DEV_NAME, nrf5_flash_init,
	     NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);


#include "../../../subsys/bluetooth/controller/ticker/ticker.h"
#include "../../../subsys/bluetooth/controller/hal/radio.h"

#define FLASH_INTERVAL 1000000UL // 1 sec  @ todo FLASH_PAGE_ERASE_MAX_TIME_US
#define FLASH_SLOT     100 /* 100 us */

extern u8_t ll_flash_ticker_id_get(); // from ll.c
extern void radio_state_abort(void); /* BLE controller abort intf. */

typedef void (*flash_op_handler_t) (void* context);

typedef struct {
	flash_op_handler_t p_op_handler;
	void * p_op_context; // [in,out]
	int result;
} flash_op_desc_t;





static void time_slot_callback_work(u32_t ticks_at_expire, u32_t remainder, u16_t lazy,
		void *context)
{
	if (radio_is_idle()) {
		((flash_op_desc_t*)context)->p_op_handler(((flash_op_desc_t*)context)->p_op_context);
		((flash_op_desc_t*)context)->result = 0;
	} else {
		printk("Error: radio not idle!\n");
		((flash_op_desc_t*)context)->result = EBUSY; // @todo error code
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
			ticks_at_expire, /* current tick */
			TICKER_US_TO_TICKS(500), /* first int. */
		   	TICKER_US_TO_TICKS(FLASH_INTERVAL), /* periodic */ //@todo 0
		   	TICKER_REMAINDER(FLASH_INTERVAL), /* per. remaind.*/ //@todo 0
		   	0, /* lazy, voluntary skips */
		   	TICKER_US_TO_TICKS(FLASH_SLOT), //@todo 0
			time_slot_callback_work,
			context,
		   	NULL, /* no op callback */
		   	NULL);

		if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
				printk("Failed to start 2nd ticker %d.\n",err);
				((flash_op_desc_t*)context)->result = -ECANCELED; // @todo erro code?
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

	/* lock resources */
	if (k_sem_take(&flash_busy_nrf5_sem, 0) != 0) {
	        printk("flash driver is locked!\n");
	        return -ENOLCK; // @todo error code
	}

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
			   TICKER_US_TO_TICKS(FLASH_INTERVAL/100), /* first int. */ // @todo 0
			   TICKER_US_TO_TICKS(FLASH_INTERVAL), /* periodic */
			   TICKER_REMAINDER(FLASH_INTERVAL), /* per. remaind.*/
			   0, /* lazy, voluntary skips */
			   TICKER_US_TO_TICKS(FLASH_SLOT),
			   time_slot_callback_helper,
			   p_flash_op_desc,
			   NULL, /* no op callback */
			   NULL);

	int result;

	if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
		printk("Failed to start ticker 1.\n");
		result = -ECANCELED; // @todo erro code?
	} else if (k_sem_take(&flash_nrf5_sem, K_MSEC(200)) != 0) { /* wait for operation's complete */
	        printk("flash_nrf5_sem not available!\n");
	        result = -ETIMEDOUT; // @todo error code
	} else {
		result = p_flash_op_desc->result;
	}

	/* unlock resources */
	k_sem_give(&flash_busy_nrf5_sem);

	return result;
}


typedef struct {
	u32_t addr; /* Address off the 1st page to erase */
	u32_t size; /* Size off area to erase [B] */
	u8_t  enable_time_limit;
} erase_context_t;

void erase_op_func(void * context)
{
	u32_t ticks_diff;
	u32_t ticks_begin       = ticker_ticks_now_get();
	u32_t pg_size           = NRF_FICR->CODEPAGESIZE;
	erase_context_t * e_ctx = context;
	u32_t i                 = 0;


	/* Erase uses a specific configuration register */
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();

	do	{
		NRF_NVMC->ERASEPAGE = e_ctx->addr;
		nvmc_wait_ready();

		printk("Erase page %d in timeslot done.\n", e_ctx->addr);

		e_ctx->size -= pg_size;
		e_ctx->addr += pg_size;
		i++;

		if (e_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((ticks_diff + ticks_diff/i) > FLASH_SLOT) {
				break;
			}
		}

	} while (e_ctx->size > 0);

	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();
}

typedef struct {
	u32_t data_addr;
	u32_t flash_addr; /* Address off the 1st page to erase */
	u32_t len; /* Size off area to erase [B] */
	u8_t  enable_time_limit;
} write_context_t;

static void shift_write_context(u32_t shift, write_context_t * w_ctx)
{
	w_ctx->flash_addr += shift;
	w_ctx->data_addr += shift;
	w_ctx->len -= shift;
}

void write_op_func(void * context)
{
	u32_t ticks_diff;
	u32_t ticks_begin       = ticker_ticks_now_get();
	write_context_t * w_ctx = context;
	u32_t addr_word;
	u32_t tmp_word;
	u32_t count;
	u32_t i = 1;

	/* Start with a word-aligned address and handle the offset */
	addr_word = (u32_t)w_ctx->flash_addr & ~0x3;

	/* If not aligned, read first word, update and write it back */
	if (!is_aligned_32(w_ctx->flash_addr)) {
		tmp_word = *(u32_t *)(addr_word);
		count = sizeof(u32_t) - (w_ctx->flash_addr & 0x3);

		if (count > w_ctx->len) {
			count = w_ctx->len;
		}

		memcpy((u8_t *)&tmp_word + (w_ctx->flash_addr & 0x3), (void *)w_ctx->data_addr, count);
		nvmc_wait_ready();
		*(u32_t *)addr_word = tmp_word;

		shift_write_context(count, w_ctx);

		if (w_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((2 * ticks_diff) > FLASH_SLOT) {
				nvmc_wait_ready();
				return;
			}
		}
	}

	/* Write all the 4-byte aligned data */
	while (w_ctx->len >= sizeof(u32_t)) {
		nvmc_wait_ready();
		*(u32_t *)w_ctx->flash_addr = *(u32_t *)w_ctx->data_addr;

		shift_write_context(sizeof(u32_t), w_ctx);

		if (w_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((ticks_diff + ticks_diff/i) > FLASH_SLOT) {
				nvmc_wait_ready();
				return;
			}
		}
	}

	/* Write remaining data */
	if (w_ctx->len) {
		tmp_word = *(u32_t *)(w_ctx->flash_addr);
		memcpy((u8_t *)&tmp_word, (void *)w_ctx->data_addr, w_ctx->len);
		nvmc_wait_ready();
		*(u32_t *)w_ctx->flash_addr = tmp_word;

		shift_write_context(w_ctx->len, w_ctx);
	}

	nvmc_wait_ready();
}


int erase_in_timeslot(u32_t addr, u32_t size)
{
	erase_context_t context = {
			addr,
			size,
			1 /* enable time limit */
	};

	flash_op_desc_t flash_op_desc = {
			erase_op_func,
			&context
	};

	int result;

	do {
		result = work_in_time_slot(&flash_op_desc);
		if (result != 0) {
			return result;
		}
	} while (context.size > 0); // Loop if operation need addidtiona timeslot(s) to been done.

	return 0;
}

int erase_in_current_context(u32_t addr, u32_t size)
{
	erase_context_t context = {
			addr,
			size,
			0 /* disable time limit */
	};

	erase_op_func(&context);

	return 0;
}

int write_in_timeslot(off_t addr, const void *data, size_t len)
{
	write_context_t context = {
			(u32_t) data,
			addr,
			len,
			1 /* enable time limit */
	};

	flash_op_desc_t flash_op_desc = {
			write_op_func,
			&context
	};

	int result;

	do {
		result = work_in_time_slot(&flash_op_desc);
		if (result != 0) {
			return result;
		}
	} while (context.len > 0); // Loop if operation need addidtiona timeslot(s) to been done.

	return 0;
}

int write_in_current_context(off_t addr, const void *data, size_t len)
{
	write_context_t context = {
			(u32_t) data,
			addr,
			len,
			0 /* enable time limit */
	};

	write_op_func(&context);

	return 0;
}
