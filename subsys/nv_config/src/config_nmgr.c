/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <limits.h>
#include "cborattr/cborattr.h"
#include "mgmt/mgmt.h"

#include "config/config.h"
#include "config_priv.h"

static int conf_nmgr_read(struct mgmt_ctxt *);
static int conf_nmgr_write(struct mgmt_ctxt *);

static const struct mgmt_handler conf_nmgr_handlers[] = {
	[CONF_NMGR_OP] = { conf_nmgr_read, conf_nmgr_write}
};

static struct mgmt_group conf_nmgr_group = {
	.mg_handlers = (struct mgmt_handler *)conf_nmgr_handlers,
	.mg_handlers_count = 1,
	.mg_group_id = MGMT_GROUP_ID_CONFIG
};

static int conf_nmgr_read(struct mgmt_ctxt *cb)
{
	int rc;
	char name_str[CONF_MAX_NAME_LEN];
	char val_str[CONF_MAX_VAL_LEN];
	char *val;
	CborError g_err = CborNoError;

	const struct cbor_attr_t attr[2] = {
		[0] = {
			.attribute = "name",
			.type = CborAttrTextStringType,
			.addr.string = name_str,
			.len = sizeof(name_str)
		},
		[1] = {
			.attribute = NULL
		}
	};

	rc = cbor_read_object(&cb->it, attr);
	if (rc) {
		return MGMT_ERR_EINVAL;
	}

	val = conf_get_value(name_str, val_str, sizeof(val_str));
	if (!val) {
		return MGMT_ERR_EINVAL;
	}

	g_err |= cbor_encode_text_stringz(&cb->encoder, "val");
	g_err |= cbor_encode_text_stringz(&cb->encoder, val);

	if (g_err) {
		return MGMT_ERR_ENOMEM;
	}
	return 0;
}

static int conf_nmgr_write(struct mgmt_ctxt *cb)
{
	int rc;
	char name_str[CONF_MAX_NAME_LEN];
	char val_str[CONF_MAX_VAL_LEN];

	rc = conf_cbor_line(cb, name_str, sizeof(name_str), val_str,
	  sizeof(val_str));
	if (rc) {
		return MGMT_ERR_EINVAL;
	}

	rc = conf_set_value(name_str, val_str);
	if (rc) {
		return MGMT_ERR_EINVAL;
	}

	rc = conf_commit(NULL);
	if (rc) {
		return MGMT_ERR_EINVAL;
	}
	return 0;
}

int
conf_nmgr_register(void)
{
	mgmt_register_group(&conf_nmgr_group);
}
