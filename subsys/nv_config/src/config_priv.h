/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_PRIV_H_
#define __CONFIG_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

int conf_cli_register(void);
int conf_nmgr_register(void);

struct mgmt_ctxt;
int conf_cbor_line(struct mgmt_ctxt *cb, char *name, int nlen, char *value,
		   int vlen);

int conf_line_parse(char *buf, char **namep, char **valp);
int conf_line_make(char *dst, int dlen, const char *name, const char *val);
int conf_line_make2(char *dst, int dlen, const char *name, const char *value);

/*
 * API for config storage.
 */
typedef void (*load_cb)(char *name, char *val, void *cb_arg);
struct conf_store_itf {
	int (*csi_load)(struct conf_store *cs, load_cb cb, void *cb_arg);
	int (*csi_save_start)(struct conf_store *cs);
	int (*csi_save)(struct conf_store *cs, const char *name,
			const char *value);
	int (*csi_save_end)(struct conf_store *cs);
};

void conf_src_register(struct conf_store *cs);
void conf_dst_register(struct conf_store *cs);

extern sys_slist_t conf_load_srcs;
extern sys_slist_t conf_handlers;
extern struct conf_store *conf_save_dst;

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_PRIV_H_ */
