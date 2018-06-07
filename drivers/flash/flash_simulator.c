/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <flash.h>
#include <init.h>
#include <kernel.h>
#include <misc/util.h>
#include <random/rand32.h>
#include <stats.h>
#include <string.h>

#if (CONFIG_FLASH_SIMULATOR_ERASE_UNIT % CONFIG_FLASH_SIMULATOR_PROG_UNIT)
#error "Erase unit must be a multiple of program unit"
#endif

#define FLASH(addr) (mock_flash + (addr)-CONFIG_FLASH_SIMULATOR_BASE_OFFSET)

#define FLASH_SIZE                                                             \
	(CONFIG_FLASH_SIMULATOR_FLASH_SIZE * CONFIG_FLASH_SIMULATOR_ERASE_UNIT)

#define STATS_SECT_EC(N, _) STATS_SECT_ENTRY32(erase_cycles_unit##N)
#define STATS_NAME_EC(N, _) STATS_NAME(flash_sim_stats, erase_cycles_unit##N)

#define STATS_SECT_DIRTYR(N, _) STATS_SECT_ENTRY32(dirty_read_unit##N)
#define STATS_NAME_DIRTYR(N, _) STATS_NAME(flash_sim_stats, dirty_read_unit##N)

/* retrieve a unit erase cycles counter */
#define ERASE_CYCLES_GET(U) (*(&flash_sim_stats.erase_cycles_unit0 + (U)))
/* increment a unit erase cycles counter */
#define ERASE_CYCLES_INC(U) (*(&flash_sim_stats.erase_cycles_unit0 + (U)) += 1)
/* increment a unit "dirty" read counter */
#define DIRTY_READ_INC(U) (*(&flash_sim_stats.dirty_read_unit0 + (U)) += 1)

/* simulator statistcs */

STATS_SECT_START(flash_sim_stats)
STATS_SECT_ENTRY32(bytes_read)		/* total bytes read */
STATS_SECT_ENTRY32(bytes_written)       /* total bytes written */
STATS_SECT_ENTRY32(double_writes)       /* num. of writes to non-erased units */
STATS_SECT_ENTRY32(flash_read_calls)    /* calls to flash_read() */
STATS_SECT_ENTRY32(flash_read_errors)   /* errors during read */
STATS_SECT_ENTRY32(flash_read_time_us)  /* time spent in flash_read() */
STATS_SECT_ENTRY32(flash_write_calls)   /* calls to flash_write() */
STATS_SECT_ENTRY32(flash_write_errors)  /* errors during write */
STATS_SECT_ENTRY32(flash_write_time_us) /* time spent in flash_write() */
STATS_SECT_ENTRY32(flash_erase_calls)   /* calls to flash_erase() */
STATS_SECT_ENTRY32(flash_erase_errors)  /* errors during erase */
STATS_SECT_ENTRY32(flash_erase_time_us) /* time spent in flash_erase() */
/* -- per-unit statistics -- */
/* erase cycle count for unit */
UTIL_EVAL(UTIL_REPEAT(CONFIG_FLASH_SIMULATOR_FLASH_SIZE, STATS_SECT_EC))
/* number of read operations on worn out erase units */
UTIL_EVAL(UTIL_REPEAT(CONFIG_FLASH_SIMULATOR_FLASH_SIZE, STATS_SECT_DIRTYR))
STATS_SECT_END;

STATS_SECT_DECL(flash_sim_stats) flash_sim_stats;
STATS_NAME_START(flash_sim_stats)
STATS_NAME(flash_sim_stats, bytes_read)
STATS_NAME(flash_sim_stats, bytes_written)
STATS_NAME(flash_sim_stats, double_writes)
STATS_NAME(flash_sim_stats, flash_read_calls)
STATS_NAME(flash_sim_stats, flash_read_errors)
STATS_NAME(flash_sim_stats, flash_read_time_us)
STATS_NAME(flash_sim_stats, flash_write_calls)
STATS_NAME(flash_sim_stats, flash_write_errors)
STATS_NAME(flash_sim_stats, flash_write_time_us)
STATS_NAME(flash_sim_stats, flash_erase_calls)
STATS_NAME(flash_sim_stats, flash_erase_errors)
STATS_NAME(flash_sim_stats, flash_erase_time_us)
UTIL_EVAL(UTIL_REPEAT(CONFIG_FLASH_SIMULATOR_FLASH_SIZE, STATS_NAME_EC))
UTIL_EVAL(UTIL_REPEAT(CONFIG_FLASH_SIMULATOR_FLASH_SIZE, STATS_NAME_DIRTYR))
STATS_NAME_END(flash_sim_stats);

static u8_t mock_flash[FLASH_SIZE];
static bool write_protection;
static u64_t rand_seed;

static const struct flash_driver_api flash_sim_api;

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_FAILURES
static int toss(int failure_rate_pm)
{
	rand_seed = 1103515245 * rand_seed + 12345;
	return ((u32_t)rand_seed % 1000) < failure_rate_pm ? -EIO : 0;
}
#endif

static int flash_range_is_valid(struct device *dev, off_t offset, size_t len)
{
	ARG_UNUSED(dev);
	if (offset + len > FLASH_SIZE + CONFIG_FLASH_SIMULATOR_BASE_OFFSET) {
		return -EINVAL;
	}

	return 1;
}

static int flash_wp_set(struct device *dev, bool enable)
{
	ARG_UNUSED(dev);
	write_protection = enable;

	return 0;
}

static bool flash_wp_is_set(void)
{
	return write_protection;
}

static int flash_sim_read(struct device *dev, const off_t offset, void *data,
			  const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	STATS_INC(flash_sim_stats, flash_read_calls);

	memcpy(data, FLASH(offset), len);
	STATS_INCN(flash_sim_stats, bytes_read, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_FAILURES

	/* for every erase unit we are reading data from, check if it wore out.
	 * if the unit wore out or if an error is randomly generated,
	 * toggle one of the bytes copied from that unit, at random
	 */

	size_t rlen = len;
	off_t toff = offset - CONFIG_FLASH_SIMULATOR_BASE_OFFSET;

	while (rlen) {

		int err;

		/* number of bytes copied from current unit */
		const size_t tlen =
		    (rlen + (toff % CONFIG_FLASH_SIMULATOR_ERASE_UNIT) >
		     CONFIG_FLASH_SIMULATOR_ERASE_UNIT)
			? (CONFIG_FLASH_SIMULATOR_ERASE_UNIT -
			   (toff % CONFIG_FLASH_SIMULATOR_ERASE_UNIT))
			: rlen;

		/* current unit */
		const u32_t unit = (toff - CONFIG_FLASH_SIMULATOR_BASE_OFFSET) /
				   CONFIG_FLASH_SIMULATOR_ERASE_UNIT;

		if (ERASE_CYCLES_GET(unit) >
		    CONFIG_FLASH_SIMULATOR_MAX_ERASE_CYCLES) {
			/* unit has worn out */
			err = 1;
			DIRTY_READ_INC(unit);
		} else {
			/* randomly generate an error */
			err = toss(CONFIG_FLASH_SIMULATOR_READ_FAILURE_RATE_PM);
			if (err) {
				STATS_INC(flash_sim_stats, flash_read_errors);
			}
		}

		if (err) {
			/* toggle a byte in the output buffer */
			off_t const rand_byte_off = (sys_rand32_get() % tlen);
			*((u8_t *)data + (len - rlen) + rand_byte_off) ^= 0xFF;
		}

		rlen -= tlen;
		toff += tlen;
	}
#endif

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
	STATS_INCN(flash_sim_stats, flash_read_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
#endif

	return 0;
}

static int flash_sim_write(struct device *dev, const off_t offset,
			   const void *data, const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	if (offset % CONFIG_FLASH_SIMULATOR_PROG_UNIT) {
		return -EINVAL;
	}

	if (flash_wp_is_set()) {
		return -EACCES;
	}

	STATS_INC(flash_sim_stats, flash_write_calls);

	/* check if any unit has been already programmed */
	for (u32_t i = 0; i < len; i += CONFIG_FLASH_SIMULATOR_PROG_UNIT) {

		u8_t buf[CONFIG_FLASH_SIMULATOR_PROG_UNIT];

		memset(buf, 0xFF, sizeof(buf));
		if (memcmp(buf, FLASH(offset + i), sizeof(buf))) {
			STATS_INC(flash_sim_stats, double_writes);
#if !CONFIG_FLASH_SIMULATOR_DOUBLE_WRITES
			return -EIO;
#endif
		}
	}

	for (u32_t i = 0; i < len; i++) {
		/* only pull bits to zero */
		*(FLASH(offset + i)) &= *((u8_t *)data + i);
	}

	STATS_INCN(flash_sim_stats, bytes_written, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_FAILURES
	/* randomly toggle a byte in flash */
	if (toss(CONFIG_FLASH_SIMULATOR_WRITE_FAILURE_RATE_PM)) {
		*(FLASH(offset) + (sys_rand32_get() % len)) ^= 0xFF;
		STATS_INC(flash_sim_stats, flash_write_errors);
	}
#endif

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
	STATS_INCN(flash_sim_stats, flash_write_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
#endif

	return 0;
}

static void unit_erase(const u32_t unit)
{
	const off_t unit_addr = CONFIG_FLASH_SIMULATOR_BASE_OFFSET +
				(unit * CONFIG_FLASH_SIMULATOR_ERASE_UNIT);

	/* byte pattern to fill the flash with */
	u8_t byte_pattern = 0xFF;

	/* erase the memory unit by pulling all bits to one */
	memset(FLASH(unit_addr), byte_pattern,
	       CONFIG_FLASH_SIMULATOR_ERASE_UNIT);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_FAILURES
	/* check if the erase cycle limit has been reached, if not
	 * check for any generated errors; update the byte pattern accordingly
	 */
	if (ERASE_CYCLES_GET(unit) > CONFIG_FLASH_SIMULATOR_MAX_ERASE_CYCLES) {
		byte_pattern = CONFIG_FLASH_SIMULATOR_MAX_ERASE_CYCLES_PATTERN;
	} else if (toss(CONFIG_FLASH_SIMULATOR_ERASE_FAILURE_RATE_PM)) {
		byte_pattern = CONFIG_FLASH_SIMULATOR_ERASE_FAILURE_PATTERN;
		STATS_INC(flash_sim_stats, flash_erase_errors);
	}

	/* fill a random area with selected pattern, if necessary */
	if (byte_pattern != 0xFF) {
		off_t area =
		    sys_rand32_get() % CONFIG_FLASH_SIMULATOR_ERASE_UNIT;
		memset(FLASH(unit_addr + (area / 2)), byte_pattern,
		       CONFIG_FLASH_SIMULATOR_ERASE_UNIT - (area / 2));
	}
#endif
}

static int flash_sim_erase(struct device *dev, const off_t offset,
			   const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

#ifdef CONFIG_FLASH_SIMULATOR_ERASE_PROTECT
	if (flash_wp_is_set()) {
		return -EACCES;
	}
#endif
	/* erase operation must be aligned to the erase unit boundary */
	if ((offset % CONFIG_FLASH_SIMULATOR_ERASE_UNIT) ||
	    (len % CONFIG_FLASH_SIMULATOR_ERASE_UNIT)) {
		return -EINVAL;
	}

	STATS_INC(flash_sim_stats, flash_erase_calls);

	/* the first unit to be erased */
	u32_t unit_start = (offset - CONFIG_FLASH_SIMULATOR_BASE_OFFSET) /
			   CONFIG_FLASH_SIMULATOR_ERASE_UNIT;

	/* erase as many units as necessary and increase their erase counter */
	for (u32_t i = 0; i < len / CONFIG_FLASH_SIMULATOR_ERASE_UNIT; i++) {
		ERASE_CYCLES_INC(unit_start + i);
		unit_erase(unit_start + i);
	}

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
	STATS_INCN(flash_sim_stats, flash_erase_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
#endif

	return 0;
}

#ifdef CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_sim_pages_layout = {
	.pages_count = CONFIG_FLASH_SIMULATOR_FLASH_SIZE,
	.pages_size = CONFIG_FLASH_SIMULATOR_ERASE_UNIT,
};

static void flash_sim_page_layout(struct device *dev,
				  const struct flash_pages_layout **layout,
				  size_t *layout_size)
{
	*layout = &flash_sim_pages_layout;
	*layout_size = 1;
}
#endif

static const struct flash_driver_api flash_sim_api = {
	.read = flash_sim_read,
	.write = flash_sim_write,
	.erase = flash_sim_erase,
	.write_protection = flash_wp_set,
	.write_block_size = CONFIG_FLASH_SIMULATOR_PROG_UNIT,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_sim_page_layout,
#endif
};

static int flash_init(struct device *dev)
{
	STATS_INIT_AND_REG(flash_sim_stats, STATS_SIZE_32, "flash_sim_stats");
	memset(mock_flash, 0xFF, ARRAY_SIZE(mock_flash));
	rand_seed = sys_rand32_get();

	return 0;
}

DEVICE_AND_API_INIT(flash_simulator, "FLASH_SIMULATOR", flash_init, NULL, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &flash_sim_api);
