/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SYS_CONFIG_FILE_H_
#define __SYS_CONFIG_FILE_H_

#include "config/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONF_FILE_NAME_MAX 32 /* max length for config filename */

struct conf_file {
	struct conf_store cf_store;
	const char *cf_name;	/* filename */
	int cf_maxlines;	/* max # of lines before compressing */
	int cf_lines;		/* private */
};

/* register file to be source of cfg */
int conf_file_src(struct conf_file *cf);

/* cfg saves go to a file */
int conf_file_dst(struct conf_file *cf);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_FILE_H_ */
