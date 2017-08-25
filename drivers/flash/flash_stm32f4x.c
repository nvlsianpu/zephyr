/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <flash.h>
#include <init.h>
#include <soc.h>

#include <flash_stm32.h>

bool flash_stm32_valid_range(off_t offset, u32_t len, bool write)
{
	ARG_UNUSED(write);
	return offset >= 0 && (offset + len - 1 <= STM32F4X_FLASH_END);
}

static int write_byte(off_t offset, u8_t val, struct flash_stm32_priv *p)
{
	struct stm32f4x_flash *regs = p->regs;
	u32_t tmp;
	int rc;

	/* if the control register is locked, do not fail silently */
	if (regs->cr & FLASH_CR_LOCK) {
		return -EIO;
	}

	rc = flash_stm32_wait_flash_idle(p);
	if (rc < 0) {
		return rc;
	}

	regs->cr &= ~CR_PSIZE_MASK;
	regs->cr |= FLASH_PSIZE_BYTE;
	regs->cr |= FLASH_CR_PG;

	/* flush the register write */
	tmp = regs->cr;

	*((u8_t *) offset + CONFIG_FLASH_BASE_ADDRESS) = val;

	rc = flash_stm32_wait_flash_idle(p);
	regs->cr &= (~FLASH_CR_PG);

	return rc;
}

static int erase_sector(u16_t sector, struct flash_stm32_priv *p)
{
	struct stm32f4x_flash *regs = p->regs;
	u32_t tmp;
	int rc;

	/* if the control register is locked, do not fail silently */
	if (regs->cr & FLASH_CR_LOCK) {
		return -EIO;
	}

	rc = flash_stm32_wait_flash_idle(p);
	if (rc < 0) {
		return rc;
	}

	regs->cr &= STM32F4X_SECTOR_MASK;
	regs->cr |= FLASH_CR_SER | (sector << 3);
	regs->cr |= FLASH_CR_STRT;

	/* flush the register write */
	tmp = regs->cr;

	rc = flash_stm32_wait_flash_idle(p);
	regs->cr &= ~(FLASH_CR_SER | FLASH_CR_SNB);

	return rc;
}

int flash_stm32_block_erase_loop(struct device *dev, unsigned int offset,
				 unsigned int len, struct flash_stm32_priv *p)
{
	struct flash_pages_info info;
	int idx, rc = 0;

	rc = flash_get_page_info_by_offs(dev, offset, &info);
	idx = info.index; /* First sector's index to be earsed. */

	rc = flash_get_page_info_by_offs(dev, offset + len - 1, &info);
	/* Now info.index has value of last sector's index to be earsed. */

	for (; idx <= info.index; idx++) {
		rc = erase_sector(idx, p);
		if (rc < 0) {
			break;
		}
	}

	return rc;
}


int flash_stm32_write_range(unsigned int offset, const void *data,
			    unsigned int len, struct flash_stm32_priv *p)
{
	int i, rc = 0;

	for (i = 0; i < len; i++, offset++) {
		rc = write_byte(offset, ((const u8_t *) data)[i], p);
		if (rc < 0) {
			return rc;
		}
	}

	return rc;
}

static const struct flash_pages_layout stm32f4xx_sectors_desc[] = {
	{.pages_count = 4, .pages_size = KB(16)},
	{.pages_count = 1, .pages_size = KB(64)},
#ifdef CONFIG_SOC_STM32F401XE
	{.pages_count = 3, .pages_size = KB(128)},
#else
	{.pages_count = 2, .pages_size = KB(128)},
#endif
};

void flash_stm32_pages_layout(struct device *dev,
			      const struct flash_pages_layout **layout,
			      size_t *layout_size)
{
	*layout = stm32f4xx_sectors_desc;
	*layout_size = ARRAY_SIZE(stm32f4xx_sectors_desc);
}
