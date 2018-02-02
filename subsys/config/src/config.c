/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <errno.h>

#include "mbedtls/base64.h"

#include "config/config.h"
#include "config_priv.h"
#include <zephyr/types.h>

/* mbedtls-base64 lib encodes data to null-terminated string */
#define BASE64_ENCODE_SIZE(in_size) ((((((in_size) - 1) / 3) * 4) + 4) + 1)

sys_slist_t conf_handlers;

static u8_t conf_cmd_inited;

void conf_init(void)
{
	sys_slist_init(&conf_handlers);
	conf_store_init();

	if (conf_cmd_inited) {
		return;
	}

	conf_cmd_inited = 1;
}

int conf_register(struct conf_handler *handler)
{
	sys_slist_prepend(&conf_handlers, &handler->ch_list);
	return 0;
}

/*
 * Find conf_handler based on name.
 */
struct conf_handler *conf_handler_lookup(char *name)
{
	struct conf_handler *ch;

	SYS_SLIST_FOR_EACH_CONTAINER(&conf_handlers, ch, ch_list) {
		if (!strcmp(name, ch->ch_name)) {
			return ch;
		}
	}
	return NULL;
}

/*
 * Separate string into argv array.
 */
int conf_parse_name(char *name, int *name_argc, char *name_argv[])
{
	char *tok;
	char *tok_ptr;
	char *sep = CONF_NAME_SEPARATOR;
	int i;

	tok = strtok_r(name, sep, &tok_ptr);

	i = 0;
	while (tok) {
		name_argv[i++] = tok;
		tok = strtok_r(NULL, sep, &tok_ptr);
	}
	*name_argc = i;

	return 0;
}

static struct conf_handler *conf_parse_and_lookup(char *name, int *name_argc,
						  char *name_argv[])
{
	int rc;

	rc = conf_parse_name(name, name_argc, name_argv);
	if (rc) {
		return NULL;
	}
	return conf_handler_lookup(name_argv[0]);
}

int conf_value_from_str(char *val_str, enum conf_type type, void *vp,
			int maxlen)
{
	s32_t val;
	s64_t val64;
	char *eptr;

	if (!val_str) {
		goto err;
	}
	switch (type) {
	case CONF_INT8:
	case CONF_INT16:
	case CONF_INT32:
	case CONF_BOOL:
		val = strtol(val_str, &eptr, 0);
		if (*eptr != '\0') {
			goto err;
		}
		if (type == CONF_BOOL) {
			if (val < 0 || val > 1) {
				goto err;
			}
			*(bool *)vp = val;
		} else if (type == CONF_INT8) {
			if (val < INT8_MIN || val > UINT8_MAX) {
				goto err;
			}
			*(int8_t *)vp = val;
		} else if (type == CONF_INT16) {
			if (val < INT16_MIN || val > UINT16_MAX) {
				goto err;
			}
			*(int16_t *)vp = val;
		} else if (type == CONF_INT32) {
			*(s32_t *)vp = val;
		}
		break;
	case CONF_INT64:
		val64 = strtoll(val_str, &eptr, 0);
		if (*eptr != '\0') {
			goto err;
		}
		*(s64_t *)vp = val64;
		break;
	case CONF_STRING:
		val = strlen(val_str);
		if (val + 1 > maxlen) {
			goto err;
		}
		strcpy(vp, val_str);
		break;
	default:
		goto err;
	}
	return 0;
err:
	return -EINVAL;
}

int conf_bytes_from_str(char *val_str, void *vp, int *len)
{
	int err;
	int rc;

	err = mbedtls_base64_decode(vp, *len, &rc, val_str, strlen(val_str));

	if (err) {
		return -1;
	}

	*len = rc;
	return 0;
}
void s64_to_dec(char *ptr, int buf_len, s64_t value, int base);
char *conf_str_from_value(enum conf_type type, void *vp, char *buf,
			  int buf_len)
{
	s32_t val;

	if (type == CONF_STRING) {
		return vp;
	}
	switch (type) {
	case CONF_INT8:
	case CONF_INT16:
	case CONF_INT32:
	case CONF_BOOL:
		if (type == CONF_BOOL) {
			val = *(bool *)vp;
		} else if (type == CONF_INT8) {
			val = *(int8_t *)vp;
		} else if (type == CONF_INT16) {
			val = *(int16_t *)vp;
		} else {
			val = *(s32_t *)vp;
		}
		snprintf(buf, buf_len, "%ld", (long)val);
		return buf;
	case CONF_INT64:
		s64_to_dec(buf, buf_len, *(s64_t *)vp, 10);
		return buf;
	default:
		return NULL;
	}
}

void u64_to_dec(char *ptr, int buf_len, u64_t value, int base)
{
	u64_t t = 0, res = 0;
	u64_t tmp = value;
	int count = 0;

	if (ptr == NULL) {
		return;
	}

	if (tmp == 0) {
		count++;
	}

	while (tmp > 0) {
		tmp = tmp/base;
		count++;
	}

	ptr += count;

	*ptr = '\0';

	do {
		res = value - base * (t = value / base);
		if (res < 10) {
			*--ptr = '0' + res;
		} else if ((res >= 10) && (res < 16)) {
			*--ptr = 'A' - 10 + res;
		}
		value = t;
	} while (value != 0);
}

void s64_to_dec(char *ptr, int buf_len, s64_t value, int base)
{
	u64_t val64;

	if (ptr == NULL || buf_len < 1) {
		return;
	}

	if (value < 0) {
		*ptr = '-';
		ptr++;
		buf_len--;
		val64 = value * (-1);
	} else {
		val64 = value;
	}

	u64_to_dec(ptr, buf_len, val64, base);
}

char *conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len)
{
	if (BASE64_ENCODE_SIZE(vp_len) > buf_len) {
		return NULL;
	}

	size_t enc_len;

	mbedtls_base64_encode(buf, buf_len, &enc_len, vp, vp_len);

	return buf;
}

int conf_set_value(char *name, char *val_str)
{
	int name_argc;
	char *name_argv[CONF_MAX_DIR_DEPTH];
	struct conf_handler *ch;

	ch = conf_parse_and_lookup(name, &name_argc, name_argv);
	if (!ch) {
		return -EINVAL;
	}

	return ch->ch_set(name_argc - 1, &name_argv[1], val_str);
}

/*
 * Get value in printable string form. If value is not string, the value
 * will be filled in *buf.
 * Return value will be pointer to beginning of that buffer,
 * except for string it will pointer to beginning of string.
 */
char *conf_get_value(char *name, char *buf, int buf_len)
{
	int name_argc;
	char *name_argv[CONF_MAX_DIR_DEPTH];
	struct conf_handler *ch;

	ch = conf_parse_and_lookup(name, &name_argc, name_argv);
	if (!ch) {
		return NULL;
	}

	if (!ch->ch_get) {
		return NULL;
	}
	return ch->ch_get(name_argc - 1, &name_argv[1], buf, buf_len);
}

int conf_commit(char *name)
{
	int name_argc;
	char *name_argv[CONF_MAX_DIR_DEPTH];
	struct conf_handler *ch;
	int rc;
	int rc2;

	if (name) {
		ch = conf_parse_and_lookup(name, &name_argc, name_argv);
		if (!ch) {
			return -EINVAL;
		}
		if (ch->ch_commit) {
			return ch->ch_commit();
		} else {
			return 0;
		}
	} else {
		rc = 0;
		SYS_SLIST_FOR_EACH_CONTAINER(&conf_handlers, ch, ch_list) {
			if (ch->ch_commit) {
				rc2 = ch->ch_commit();
				if (!rc) {
					rc = rc2;
				}
			}
		}
		return rc;
	}
}
