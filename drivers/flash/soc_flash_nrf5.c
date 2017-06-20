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

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
#include <misc/__assert.h>
#include "controller/ticker/ticker.h"
#include "controller/hal/radio.h"
#include "controller/include/ll.h"
#endif /* CONFIG_BLUETOOTH_CONTROLLER */

#define FLASH_OP_DONE    (0) /* 0 for compilance with the driver API. */
#define FLASH_OP_ONGOING (-1)

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
#define RADIO_TICKER_IS_INITIALIZED() ticker_is_initialized(0)
#define FLASH_SLOT     FLASH_PAGE_ERASE_MAX_TIME_US
#define FLASH_INTERVAL FLASH_SLOT
#endif /* CONFIG_BLUETOOTH_CONTROLLER */

struct erase_context {
	u32_t addr; /* Address off the 1st page to erase */
	u32_t size; /* Size off area to erase [B] */
#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	u8_t enable_time_limit; /* execution limited to timeslot */
#endif
}; /*< Context type for f. @ref erase_op */

struct write_context {
	u32_t data_addr;
	u32_t flash_addr; /* Address off the 1st page to erase */
	u32_t len;        /* Size off data to write [B] */
#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	u8_t  enable_time_limit; /* execution limited to timeslot*/
#endif
}; /*< Context type for f. @ref write_op */


#if defined(CONFIG_BLUETOOTH_CONTROLLER)
typedef int (*flash_op_handler) (void *context);

struct flash_op_desc {
	flash_op_handler handler;
	void *context; /* [in,out] */
	int result;
};

/* semaphore for synchronization of flash opperations */
static struct k_sem sem_sync;
#endif /* CONFIG_BLUETOOTH_CONTROLLER */

/* semaphore for lock flash resorces (tickers) */
static struct k_sem sem_lock;

static int write(off_t addr, const void *data, size_t len);
static int erase(u32_t addr, u32_t size);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
static int write_op(void *context); /* instantation of flash_op_handler */
static int write_in_timeslice(off_t addr, const void *data, size_t len);

static int erase_op(void *context); /* instantation of flash_op_handler */
static int erase_in_timeslice(u32_t addr, u32_t size);

extern void radio_state_abort(void); /* BLE controller abort */
#endif /* CONFIG_BLUETOOTH_CONTROLLER */


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
	int ret;

	if (!is_addr_valid(addr, len)) {
		return -EINVAL;
	}

	if (!len) {
		return 0;
	}

	k_sem_take(&sem_lock, K_FOREVER);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	if (RADIO_TICKER_IS_INITIALIZED()) {
		ret = write_in_timeslice(addr, data, len);
	} else
#endif
	{
		ret = write(addr, data, len);
	}

	k_sem_give(&sem_lock);

	return ret;
}



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

	k_sem_take(&sem_lock, K_FOREVER);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	if (RADIO_TICKER_IS_INITIALIZED()) {
		ret = erase_in_timeslice(addr, size);
	} else
#endif
	{
		ret = erase(addr, size);
	}

	k_sem_give(&sem_lock);

	return ret;
}

static int flash_nrf5_write_protection(struct device *dev, bool enable)
{
	k_sem_take(&sem_lock, K_FOREVER);

	if (enable) {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
	} else {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
	}
	nvmc_wait_ready();

	k_sem_give(&sem_lock);

	return 0;
}

static const struct flash_driver_api flash_nrf5_api = {
	.read = flash_nrf5_read,
	.write = flash_nrf5_write,
	.erase = flash_nrf5_erase,
	.write_protection = flash_nrf5_write_protection,
};



static int nrf5_flash_init(struct device *dev)
{
	dev->driver_api = &flash_nrf5_api;

	k_sem_init(&sem_lock, 1, 1);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	k_sem_init(&sem_sync, 0, 1);
#endif

	return 0;
}

DEVICE_INIT(nrf5_flash, CONFIG_SOC_FLASH_NRF5_DEV_NAME, nrf5_flash_init,
	     NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);


#if defined(CONFIG_BLUETOOTH_CONTROLLER)

static void time_slot_callback_work(u32_t ticks_at_expire, u32_t remainder,
		u16_t lazy,
		void *context)
{
	int result;
	u8_t ticker_id;
	u8_t instance_index;
	struct flash_op_desc *op_desc;

	if (radio_is_idle()) {
		op_desc = context;

		if (op_desc->handler(op_desc->context)
			== FLASH_OP_DONE) {
			ll_timeslice_ticker_id_get(&instance_index, &ticker_id);

			/* Stop the time slot ticker */
			result = ticker_stop(instance_index,
					     0,
					     ticker_id,
					     NULL,
					     NULL);

			if ((result != TICKER_STATUS_SUCCESS)
			    && (result != TICKER_STATUS_BUSY)) {
				__ASSERT(0, "Failed to stop ticker.\n");
			}

			((struct flash_op_desc *)context)->result = 0;

			/* notify thread that data is available */
			k_sem_give(&sem_sync);
		}
	} else {
		__ASSERT(0, "Radio is on during flash opperation.\n");
	}
}

static void time_slot_callback_helper(u32_t ticks_at_expire, u32_t remainder,
		u16_t lazy, void *context)
{
	u8_t instance_index;
	u8_t ticker_id;
	int err;

	ll_timeslice_ticker_id_get(&instance_index, &ticker_id);


	radio_state_abort();

	/* start a secondary one-shot ticker after ~ 500 us, */
	/* this will let any radio role to gracefully release the Radio h/w */

	err = ticker_start(instance_index, /* Radio instance ticker */
		0, /* user_id */
		0, /* ticker_id */
		ticks_at_expire, /* current tick */
		TICKER_US_TO_TICKS(500), /* first int. */
		0, /* periodic (on-shot) */
		0, /* per. remaind. (on-shot) */
		0, /* lazy, voluntary skips */
		0,
		time_slot_callback_work, /* handler for exexute */
					 /* the flash operiation */
		context, /* the context for the flash operiation */
		NULL, /* no op callback */
		NULL);

	if ((err != TICKER_STATUS_SUCCESS) && (err != TICKER_STATUS_BUSY)) {
		((struct flash_op_desc *)context)->result = -ECANCELED;

		/* abort flash timeslots */
		err = ticker_stop(instance_index, 0, ticker_id, NULL, NULL);

		if ((err != TICKER_STATUS_SUCCESS)
			&& (err != TICKER_STATUS_BUSY)) {
			__ASSERT(0, "Failed to stop ticker %d.\n");
		}

		/* notify thread that data is available */
		k_sem_give(&sem_sync);
	}
}


static int work_in_time_slice(struct flash_op_desc *p_flash_op_desc)
{
	u8_t instance_index;
	u8_t ticker_id;
	u32_t err;

	ll_timeslice_ticker_id_get(&instance_index, &ticker_id);

	err = ticker_start(instance_index,
			   3, /* user id for thread mode */
			      /* (MAYFLY_CALL_ID_PROGRAM) */
			   ticker_id, /* flash ticker id */
			   ticker_ticks_now_get(), /* current tick */
			   0, /* first int. immedetely */
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
		result = -ECANCELED;
	} else if (k_sem_take(&sem_sync, K_MSEC(200)) != 0) {
		/* wait for operation's complete overrun*/
		result = -ETIMEDOUT;
	} else {
		result = p_flash_op_desc->result;
	}

	return result;
}



static int erase_in_timeslice(u32_t addr, u32_t size)
{
	struct erase_context context = {
			addr,
			size,
			1 /* enable time limit */
	};

	struct flash_op_desc flash_op_desc = {
			erase_op,
			&context
	};

	return work_in_time_slice(&flash_op_desc);
}


static int write_in_timeslice(off_t addr, const void *data, size_t len)
{
	struct write_context context = {
			(u32_t) data,
			addr,
			len,
			1 /* enable time limit */
	};

	struct flash_op_desc flash_op_desc = {
			write_op,
			&context
	};

	return  work_in_time_slice(&flash_op_desc);
}

#endif /* CONFIG_BLUETOOTH_CONTROLLER */



static int erase_op(void *context)
{
	u32_t pg_size           = NRF_FICR->CODEPAGESIZE;
	struct erase_context *e_ctx = context;

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	u32_t ticks_diff;
	u32_t ticks_begin;
	u32_t i = 0;

	if (e_ctx->enable_time_limit) {
		ticks_begin = ticker_ticks_now_get();
	}
#endif

	/* Erase uses a specific configuration register */
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();

	do	{
		NRF_NVMC->ERASEPAGE = e_ctx->addr;
		nvmc_wait_ready();

		e_ctx->size -= pg_size;
		e_ctx->addr += pg_size;

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
		i++;

		if (e_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((ticks_diff + ticks_diff/i) > FLASH_SLOT) {
				break;
			}
		}
#endif

	} while (e_ctx->size > 0);

	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
	nvmc_wait_ready();

	return (e_ctx->size > 0) ? FLASH_OP_ONGOING : FLASH_OP_DONE;
}



static void shift_write_context(u32_t shift, struct write_context *w_ctx)
{
	w_ctx->flash_addr += shift;
	w_ctx->data_addr += shift;
	w_ctx->len -= shift;
}

static int write_op(void *context)
{
	struct write_context *w_ctx = context;
	u32_t addr_word;
	u32_t tmp_word;
	u32_t count;

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
	u32_t ticks_diff;
	u32_t ticks_begin;
	u32_t i = 1;

	if (w_ctx->enable_time_limit) {
		ticks_begin = ticker_ticks_now_get();
	}
#endif

	/* Start with a word-aligned address and handle the offset */
	addr_word = (u32_t)w_ctx->flash_addr & ~0x3;

	/* If not aligned, read first word, update and write it back */
	if (!is_aligned_32(w_ctx->flash_addr)) {
		tmp_word = *(u32_t *)(addr_word);
		count = sizeof(u32_t) - (w_ctx->flash_addr & 0x3);

		if (count > w_ctx->len) {
			count = w_ctx->len;
		}

		memcpy((u8_t *)&tmp_word + (w_ctx->flash_addr & 0x3),
		       (void *)w_ctx->data_addr,
		       count);
		nvmc_wait_ready();
		*(u32_t *)addr_word = tmp_word;

		shift_write_context(count, w_ctx);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
		if (w_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((2 * ticks_diff) > FLASH_SLOT) {
				nvmc_wait_ready();
				return FLASH_OP_ONGOING;
			}
		}
#endif
	}

	/* Write all the 4-byte aligned data */
	while (w_ctx->len >= sizeof(u32_t)) {
		nvmc_wait_ready();
		*(u32_t *)w_ctx->flash_addr = *(u32_t *)w_ctx->data_addr;

		shift_write_context(sizeof(u32_t), w_ctx);

#if defined(CONFIG_BLUETOOTH_CONTROLLER)
		i++;

		if (w_ctx->enable_time_limit) {

			ticks_diff = ticker_ticks_now_get() - ticks_begin;

			if ((ticks_diff + ticks_diff/i) > FLASH_SLOT) {
				nvmc_wait_ready();
				return FLASH_OP_ONGOING;
			}
		}
#endif
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

	return FLASH_OP_DONE;
}


static int erase(u32_t addr, u32_t size)
{
	struct erase_context context = {
			addr,
			size,
#if defined(CONFIG_BLUETOOTH_CONTROLLER)
			0 /* disable time limit */
#endif
	};

	return	erase_op(&context);
}


static int write(off_t addr, const void *data, size_t len)
{
	struct write_context context = {
			(u32_t) data,
			addr,
			len,
#if defined(CONFIG_BLUETOOTH_CONTROLLER)
			0 /* enable time limit */
#endif
	};

	return write_op(&context);
}
