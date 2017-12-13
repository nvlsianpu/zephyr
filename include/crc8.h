/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * CRC8-CCITT, with normal polynomial; 0x07.
 */

#ifndef __CRC8_H_
#define __CRC8_H_

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

u8_t crc8_init(void);
u8_t crc8_ccitt(u8_t val, void *buf, int cnt);

#ifdef __cplusplus
}
#endif

#endif
