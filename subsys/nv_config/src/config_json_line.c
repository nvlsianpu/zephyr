/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config/config.h"
#include "config_priv.h"
#include "cborattr/cborattr.h"
#include "mgmt/mgmt.h"

int conf_cbor_line(struct mgmt_ctxt *cb, char *name, int nlen, char *value,
		   int vlen)
{
	const struct cbor_attr_t val_attr[3] = {
		[0] = {
			.attribute = "name",
			.type = CborAttrTextStringType,
			.addr.string = name,
			.len = nlen
		},
		[1] = {
			.attribute = "val",
			.type = CborAttrTextStringType,
			.addr.string = value,
			.len = vlen
		},
		[2] = {
			.attribute = NULL
		}
	};
	int rc;

	rc = cbor_read_object(&cb->it, val_attr);
	if (rc) {
		return rc;
	}
	return 0;
}
