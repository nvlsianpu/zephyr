/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>

#include "config/config.h"
#include "config_priv.h"

struct conf_dup_check_arg {
	const char *name;
	const char *val;
	int is_dup;
};

sys_slist_t  conf_load_srcs;
struct conf_store *conf_save_dst;

void conf_src_register(struct conf_store *cs)
{
	sys_snode_t *prev, *cur;

	prev = NULL;

	SYS_SLIST_FOR_EACH_NODE(&conf_load_srcs, cur) {
		prev = cur;
	}

	sys_slist_insert(&conf_load_srcs, prev, &cs->cs_next);
}

void conf_dst_register(struct conf_store *cs)
{
	conf_save_dst = cs;
}

static void conf_load_cb(char *name, char *val, void *cb_arg)
{
	conf_set_value(name, val);
}

int conf_load(void)
{
	struct conf_store *cs;

	/*
	 * for every config store
	 *    load config
	 *    apply config
	 *    commit all
	 */

	SYS_SLIST_FOR_EACH_CONTAINER(&conf_load_srcs, cs, cs_next) {
		cs->cs_itf->csi_load(cs, conf_load_cb, NULL);
	}
	return conf_commit(NULL);
}

static void conf_dup_check_cb(char *name, char *val, void *cb_arg)
{
	struct conf_dup_check_arg *cdca = (struct conf_dup_check_arg *)cb_arg;

	if (strcmp(name, cdca->name)) {
		return;
	}
	if (!val) {
		if (!cdca->val || cdca->val[0] == '\0') {
			cdca->is_dup = 1;
		} else {
			cdca->is_dup = 0;
		}
	} else {
		if (cdca->val && !strcmp(val, cdca->val)) {
			cdca->is_dup = 1;
		} else {
			cdca->is_dup = 0;
		}
	}
}

/*
 * Append a single value to persisted config. Don't store duplicate value.
 */
int conf_save_one(const char *name, char *value)
{
	struct conf_store *cs;
	struct conf_dup_check_arg cdca;

	cs = conf_save_dst;
	if (!cs) {
		return -ENOENT;
	}

	/*
	 * Check if we're writing the same value again.
	 */
	cdca.name = name;
	cdca.val = value;
	cdca.is_dup = 0;
	cs->cs_itf->csi_load(cs, conf_dup_check_cb, &cdca);
	if (cdca.is_dup == 1) {
		return 0;
	}
	return cs->cs_itf->csi_save(cs, name, value);
}

/*
 * Walk through all registered subsystems, and ask them to export their
 * config variables. Persist these settings.
 */
static void conf_store_one(char *name, char *value)
{
	conf_save_one(name, value);
}

int conf_save(void)
{
	struct conf_store *cs;
	struct conf_handler *ch;
	int rc;
	int rc2;

	cs = conf_save_dst;
	if (!cs) {
		return -ENOENT;
	}

	if (cs->cs_itf->csi_save_start) {
		cs->cs_itf->csi_save_start(cs);
	}
	rc = 0;

	SYS_SLIST_FOR_EACH_CONTAINER(&conf_handlers, ch, ch_list) {
		if (ch->ch_export) {
			rc2 = ch->ch_export(conf_store_one,
					    CONF_EXPORT_PERSIST);
			if (!rc) {
				rc = rc2;
			}
		}
	}
	if (cs->cs_itf->csi_save_end) {
		cs->cs_itf->csi_save_end(cs);
	}
	return rc;
}

void conf_store_init(void)
{
	sys_slist_init(&conf_load_srcs);
}
