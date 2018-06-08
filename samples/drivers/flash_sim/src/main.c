/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2016 Linaro Limited
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <flash.h>
#include <misc/util.h>
#include <stats.h>
#include <stdio.h>
#include <string.h>
#include <zephyr.h>

/* number of test iterations */
#define ITERATIONS 100

/* flash size in bytes */
#define FLASH_SIZE                                                             \
	(CONFIG_FLASH_SIMULATOR_ERASE_UNIT * CONFIG_FLASH_SIMULATOR_FLASH_SIZE)

static struct device *flash_dev;

static int flash_sim_stats_walk_fn(struct stats_hdr *hdr, void *arg,
				   const char *name, uint16_t off)
{
	printf("[%10u]\t%s\n", *(u32_t *)((u8_t *)hdr + off), name);
	return 0;
}

/* erase a unit, program it and read back the contents */
static void unit_test(u32_t addr)
{
	/* do not check error codes to avoid spam */

	(void)flash_erase(flash_dev, addr, CONFIG_FLASH_SIMULATOR_ERASE_UNIT);

	for (u32_t i = 0; i < CONFIG_FLASH_SIMULATOR_ERASE_UNIT;
	     i += CONFIG_FLASH_SIMULATOR_PROG_UNIT) {

		u8_t buf[CONFIG_FLASH_SIMULATOR_PROG_UNIT] = {0};

		(void)flash_write(flash_dev, addr + i, &buf, sizeof(buf));
		(void)flash_read(flash_dev, addr + i, &buf, sizeof(buf));
	}
}

void main(void)
{
	struct stats_hdr *sim_stats;

	printf("\nFlash simulator\n");
	printf("=========================\n");

	flash_dev = device_get_binding(SIM_FLASH_DEV_NAME);

	if (!flash_dev) {
		printf("Flash simulator driver was not found!\n");
		return;
	}

	printf("Flash base offset: \t\t0x%.8x\n",
	       CONFIG_FLASH_SIMULATOR_BASE_OFFSET);
	printf("Flash size: \t\t\t%u bytes (%u erase units)\n", FLASH_SIZE,
	       CONFIG_FLASH_SIMULATOR_FLASH_SIZE);
	printf("Erase unit: \t\t\t%u bytes\n",
	       CONFIG_FLASH_SIMULATOR_ERASE_UNIT);
	printf("Program unit: \t\t\t%u bytes\n",
	       CONFIG_FLASH_SIMULATOR_PROG_UNIT);
	printf("Allow double writes: \t\t%s\n",
	       IS_ENABLED(CONFIG_FLASH_SIMULATOR_DOUBLE_WRITES) ? "yes" : "no");
	printf("Write prot. is also erase prot: %s\n",
	       IS_ENABLED(CONFIG_FLASH_SIMULATOR_ERASE_PROTECT) ? "yes" : "no");

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_FAILURES
	printf("Failure simulation: \t\ton\n");
	printf("Maximum hardware erase cycles: \t%u\n",
	       CONFIG_FLASH_SIMULATOR_MAX_ERASE_CYCLES);
	printf("Failure rate on read: \t\t%.1f%%\n",
	       CONFIG_FLASH_SIMULATOR_READ_FAILURE_RATE_PM / 10.0);
	printf("Failure rate on write: \t\t%.1f%%\n",
	       CONFIG_FLASH_SIMULATOR_WRITE_FAILURE_RATE_PM / 10.0);
	printf("Failure rate on erase: \t\t%.1f%%\n",
	       CONFIG_FLASH_SIMULATOR_ERASE_FAILURE_RATE_PM / 10.0);
#else
	printf("Failure simulation: \t\toff\n");
#endif

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	printf("Timing simulation: \t\ton\n");
	printf("Minimum read time: \t\t%u µs\n",
	       CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
	printf("Minimum write time: \t\t%u µs\n",
	       CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
	printf("Minimum erase time: \t\t%u µs\n",
	       CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
#else
	printf("Timing simulation: \t\toff\n");
#endif

	printf("\nMaking a few API calls... (%u iterations)\n", ITERATIONS);
	printf("Each iteration a unit is selected and:\n"
	       "- erased\n"
	       "- entirely programmed, %d bytes at a time\n"
	       "- read, %d bytes at a time\n",
	       CONFIG_FLASH_SIMULATOR_PROG_UNIT,
	       CONFIG_FLASH_SIMULATOR_PROG_UNIT);

	for (u32_t i = 0; i < ITERATIONS; i++) {
		/* select a new unit every iteration */
		u32_t const unit_addr =
		    CONFIG_FLASH_SIMULATOR_BASE_OFFSET +
		    (i % CONFIG_FLASH_SIMULATOR_FLASH_SIZE) *
			CONFIG_FLASH_SIMULATOR_ERASE_UNIT;

		unit_test(unit_addr);
	}

	printf("...done\n");
	printf("\nRetrieving simulator statistics\n");

	sim_stats = stats_group_find("flash_sim_stats");
	if (sim_stats) {
		stats_walk(sim_stats, flash_sim_stats_walk_fn, NULL);
	} else {
		printf("No statistics group found!\n");
	}

	printf("\nAll tests completed.\n");
}
