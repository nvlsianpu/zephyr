/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <fcb.h>
#include <string.h>

#include "config/config.h"
#include "config/config_fcb.h"
#include "config_priv.h"

#define CONF_FCB_VERS		1

struct conf_fcb_load_cb_arg {
	load_cb cb;
	void *cb_arg;
};

static int conf_fcb_load(struct conf_store *, load_cb cb, void *cb_arg);
static int conf_fcb_save(struct conf_store *, const char *name,
			 const char *value);

static struct conf_store_itf conf_fcb_itf = {
	.csi_load = conf_fcb_load,
	.csi_save = conf_fcb_save,
};

int conf_fcb_src(struct conf_fcb *cf)
{
	int rc;

	cf->cf_fcb.f_version = CONF_FCB_VERS;
	cf->cf_fcb.f_scratch_cnt = 1;

	while (1) {
		rc = fcb_init(CONFIG_CFGSYS_FCB_FLASH_AREA, &cf->cf_fcb);
		if (rc) {
			return -EINVAL;
		}

		/*
		 * Check if system was reset in middle of emptying a sector.
		 * This situation is recognized by checking if the scratch block
		 * is missing.
		 */
		if (fcb_free_sector_cnt(&cf->cf_fcb) < 1) {

			rc = flash_area_erase(cf->cf_fcb.fap,
					cf->cf_fcb.f_active.fe_sector->fs_off,
					cf->cf_fcb.f_active.fe_sector->fs_size);

			if (rc) {
				return -EIO;
			}
		} else {
			break;
		}
	}

	cf->cf_store.cs_itf = &conf_fcb_itf;
	conf_src_register(&cf->cf_store);

	return 0;
}

int conf_fcb_dst(struct conf_fcb *cf)
{
	cf->cf_store.cs_itf = &conf_fcb_itf;
	conf_dst_register(&cf->cf_store);

	return 0;
}

static int conf_fcb_load_cb(struct fcb_entry_ctx *entry_ctx, void *arg)
{
	struct conf_fcb_load_cb_arg *argp;
	char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
	char *name_str;
	char *val_str;
	int rc;
	int len;

	argp = (struct conf_fcb_load_cb_arg *)arg;

	len = entry_ctx->loc.fe_data_len;
	if (len >= sizeof(buf)) {
		len = sizeof(buf) - 1;
	}

	rc = flash_area_read(entry_ctx->fap,
			     FCB_ENTRY_FA_DATA_OFF(entry_ctx->loc), buf, len);

	if (rc) {
		return 0;
	}
	buf[len] = '\0';

	rc = conf_line_parse(buf, &name_str, &val_str);
	if (rc) {
		return 0;
	}
	argp->cb(name_str, val_str, argp->cb_arg);
	return 0;
}

static int conf_fcb_load(struct conf_store *cs, load_cb cb, void *cb_arg)
{
	struct conf_fcb *cf = (struct conf_fcb *)cs;
	struct conf_fcb_load_cb_arg arg;
	int rc;

	arg.cb = cb;
	arg.cb_arg = cb_arg;
	rc = fcb_walk(&cf->cf_fcb, 0, conf_fcb_load_cb, &arg);
	if (rc) {
		return -EINVAL;
	}
	return 0;
}

static int conf_fcb_var_read(struct fcb_entry_ctx *entry_ctx, char *buf,
			     char **name, char **val)
{
	int rc;

	rc = flash_area_read(entry_ctx->fap,
			     FCB_ENTRY_FA_DATA_OFF(entry_ctx->loc), buf,
			     entry_ctx->loc.fe_data_len);
	if (rc) {
		return rc;
	}
	buf[entry_ctx->loc.fe_data_len] = '\0';
	rc = conf_line_parse(buf, name, val);
	return rc;
}

static void conf_fcb_compress(struct conf_fcb *cf)
{
	int rc;
	char buf1[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
	char buf2[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
	struct fcb_entry_ctx loc1;
	struct fcb_entry_ctx loc2;
	char *name1, *val1;
	char *name2, *val2;
	int copy;

	rc = fcb_append_to_scratch(&cf->cf_fcb);
	if (rc) {
		return; /* XXX */
	}

	loc1.fap = cf->cf_fcb.fap;

	loc1.loc.fe_sector = NULL;
	loc1.loc.fe_elem_off = 0;
	while (fcb_getnext(&cf->cf_fcb, &loc1.loc) == 0) {
		if (loc1.loc.fe_sector != cf->cf_fcb.f_oldest) {
			break;
		}
		rc = conf_fcb_var_read(&loc1, buf1, &name1, &val1);
		if (rc) {
			continue;
		}
		loc2 = loc1;
		copy = 1;
		while (fcb_getnext(&cf->cf_fcb, &loc2.loc) == 0) {
			rc = conf_fcb_var_read(&loc2, buf2, &name2, &val2);
			if (rc) {
				continue;
			}
			if (!strcmp(name1, name2)) {
				copy = 0;
				break;
			}
		}
		if (!copy) {
			continue;
		}

		/*
		 * Can't find one. Must copy.
		 */
		rc = flash_area_read(loc1.fap, FCB_ENTRY_FA_DATA_OFF(loc1.loc),
				     buf1, loc1.loc.fe_data_len);
		if (rc) {
			continue;
		}
		rc = fcb_append(&cf->cf_fcb, loc1.loc.fe_data_len, &loc2.loc);
		if (rc) {
			continue;
		}
		rc = flash_area_write(loc2.fap, FCB_ENTRY_FA_DATA_OFF(loc2.loc),
				      buf1, loc1.loc.fe_data_len);
		if (rc) {
			continue;
		}
		fcb_append_finish(&cf->cf_fcb, &loc2.loc);
	}
	rc = fcb_rotate(&cf->cf_fcb);

	__ASSERT(rc == 0, "Failed to fcb rotate.\n");
}

static int conf_fcb_append(struct conf_fcb *cf, char *buf, int len)
{
	int rc;
	int i;
	struct fcb_entry loc;

	for (i = 0; i < 10; i++) {
		rc = fcb_append(&cf->cf_fcb, len, &loc);
		if (rc != FCB_ERR_NOSPACE) {
			break;
		}
		conf_fcb_compress(cf);
	}
	if (rc) {
		return -EINVAL;
	}

	rc = flash_area_write(cf->cf_fcb.fap, FCB_ENTRY_FA_DATA_OFF(loc),
			      buf, len);
	if (rc) {
		return -EINVAL;
	}
	fcb_append_finish(&cf->cf_fcb, &loc);
	return 0;
}

static int conf_fcb_save(struct conf_store *cs, const char *name,
			 const char *value)
{
	struct conf_fcb *cf = (struct conf_fcb *)cs;
	char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
	int len;

	if (!name) {
		return -EINVAL;
	}

	len = conf_line_make(buf, sizeof(buf), name, value);
	if (len < 0 || len + 2 > sizeof(buf)) {
		return -EINVAL;
	}
	return conf_fcb_append(cf, buf, len);
}
