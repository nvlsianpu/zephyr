/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2016 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_H__
#define __MCUBOOT_H__

#include "boot/mcuboot_constraints.h"

/**
 * @brief Marks the image in slot 0 as confirmed. The system will continue booting
 * into the image in slot 0 until told to boot from a different slot.
 *
 * @return 0 on success, negative errno code on fail.
 */
int boot_img_confirmed_set(void);

/**
 * @brief Marks the image in slot 1 as pending. On the next reboot, the system will
 * perform a one-time boot of the slot 1 image.
 *
 * @param permanent Whether the image should be used permanently or
 * only tested once:
 *   0=run image once, then confirm or revert.
 *   1=run image forever.
 * @return 0 on success, negative errno code on fail.
 */
int boot_img_pending_set(int permanent);

/**
 * @brief Erase the image Bank.
 *
 * @param bank_offset address of the image bank
 * @return 0 on success, negative errno code on fail.
 */
int boot_img_bank_erase(u32_t bank_offset);

#endif	/* __MCUBOOT_H__ */
