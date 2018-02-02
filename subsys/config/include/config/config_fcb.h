/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SYS_CONFIG_FCB_H_
#define __SYS_CONFIG_FCB_H_

#include <fcb.h>
#include "config/config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct conf_fcb {
	struct conf_store cf_store;
	struct fcb cf_fcb;
};

extern int conf_fcb_src(struct conf_fcb *cf);
extern int conf_fcb_dst(struct conf_fcb *cf);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_FCB_H_ */
