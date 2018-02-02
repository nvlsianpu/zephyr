/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SYS_CONFIG_H_
#define __SYS_CONFIG_H_

#include <misc/util.h>
#include <misc/slist.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sys_config sys/config package
 * @{
 */

#define CONF_MAX_DIR_DEPTH	8	/* max depth of config tree */
#define CONF_MAX_NAME_LEN	(8 * CONF_MAX_DIR_DEPTH)
#define CONF_MAX_VAL_LEN	256
#define CONF_NAME_SEPARATOR	"/"

#define CONF_NMGR_OP		0

/**
 * Type of configuration value.
 */
enum conf_type {
	CONF_NONE = 0,
	CONF_DIR,
	CONF_INT8,
	CONF_INT16,
	CONF_INT32,
	CONF_INT64,
	CONF_STRING,
	CONF_BYTES,
	CONF_FLOAT,
	CONF_DOUBLE,
	CONF_BOOL,
} __attribute__((__packed__));

/**
 * Parameter to commit handler describing where data is going to.
 */
enum conf_export_tgt {
	CONF_EXPORT_PERSIST,        /* Value is to be persisted. */
	CONF_EXPORT_SHOW            /* Value is to be displayed. */
};

/**
 * @struct conf_handler
 * Config handlers for subtree implement a set of handler functions.
 * These are registered using a call to conf_register().
 *
 * @param conf_handler::ch_list Linked list node info for module internal usage.
 *
 * @param conf_handler::ch_name Name of subtree.
 *
 * @param conf_handler::ch_get Get values handler of configuration items
 * identified by keyword names.Parameters:
 *  - argc - count of item in argv.
 *  - argv - array of pointers to keyword names.
 *  - val - buffer for a value.
 *  - val_len_max - size of that buffer.
 *
 * @param conf_handler::ch_set Sey value handler of configuration items
 * identified by keyword names. Parameters:
 *  - argc - count of item in argv, argv - array of pointers to keyword names.
 *  - val- pointer to value to be set.
 *
 * @param conf_handler::ch_commit This handler gets called after configuration
 * has been loaded in full. User might us it to apply setting to
 * the application.
 *
 * @param conf_handler::ch_export This gets called to dump all current
 * configuration.
 * This happens when conf_save() tries to save the settings. Parameters:
 *  - tgt: indicates where data is going to.
 *  - Export_function: the pointer to the internal function which appends
 *   a single key-value pair to persisted config. Don't store duplicated
 *   value. The name is subtree/key string, val is the string with
 *   value.
 *
 * @remarks The User might limit a implementations of handler to serving only
 * one keyword at one call - what will impose limit to get/set values using full
 * subtree/key name.
 */
struct conf_handler {
	sys_snode_t ch_list;
	char *ch_name;
	char *(*ch_get)(int argc, char **argv, char *val, int val_len_max);
	int (*ch_set)(int argc, char **argv, char *val);
	int (*ch_commit)(void);
	int (*ch_export)(void (*export_func)(char *name, char *val),
	  enum conf_export_tgt tgt);
};

void conf_init(void);
void conf_store_init(void);

/**
 * Register a handler for configurations items.
 *
 * @param cf Structure containing registration info.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_register(struct conf_handler *cf);

/**
 * Load configuration from registered persistence sources. Handlers for
 * configuration subtrees registered earlier will be called for encountered
 * values.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_load(void);

/**
 * Save currently running configuration. All configuration which is different
 * from currently persisted values will be saved.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_save(void);

/**
 * Write a single configuration value to persisted storage (if it has
 * changed value).
 *
 * @param name Name/key of the configuration item.
 * @param var Value of the configuration item.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_save_one(const char *name, char *var);

/**
 * Set configuration item identified by @p name to be value @p val_str.
 * This finds the configuration handler for this subtree and calls it's
 * set handler.
 *
 * @param name Name/key of the configuration item.
 * @param val_str Value of the configuration item.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_set_value(char *name, char *val_str);

/**
 * Get value of configuration item identified by @p name.
 * This calls the configuration handler ch_get for the subtree.
 *
 * Configuration handler can copy the string to @p buf, the maximum
 * number of bytes it will copy is limited by @p buf_len.
 *
 * @param name Name/key of the configuration item.
 *
 * @param buf buffer for value of the configuration item.
 * If value is not string, the value will be filled in *buf.
 *
 * @param buf_len size of buf.
 *
 * @return value will be pointer to beginning of the buf,
 * except for string it will pointer to beginning of string source.
 */
char *conf_get_value(char *name, char *buf, int buf_len);

/**
 * Call commit for all configuration handler. This should apply all
 * configuration which has been set, but not applied yet.
 *
 * @param name Name of the configuration subtree, or NULL to commit everything.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_commit(char *name);

/**
 * Convenience routine for converting value passed as a string to native
 * data type.
 *
 * @param val_str Value of the configuration item as string.
 * @param type Type of the value to convert to.
 * @param vp Pointer to variable to fill with the decoded value.
 * @param maxlen the vp buffer size.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_value_from_str(char *val_str, enum conf_type type, void *vp,
			int maxlen);

/**
 * Convenience routine for converting byte array passed as a base64
 * encoded string.
 *
 * @param val_str Value of the configuration item as string.
 * @param vp Pointer to variable to fill with the decoded value.
 * @param len Size of that variable. On return the number of bytes in the array.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_bytes_from_str(char *val_str, void *vp, int *len);

/**
 * Convenience routine for converting native data type to a string.
 *
 * @param type Type of the value to convert from.
 * @param vp Pointer to variable to convert.
 * @param buf Buffer where string value will be stored.
 * @param buf_len Size of the buffer.
 *
 * @return 0 on success, non-zero on failure.
 */
char *conf_str_from_value(enum conf_type type, void *vp, char *buf,
			  int buf_len);
#define CONF_STR_FROM_BYTES_LEN(len) (((len) * 4 / 3) + 4)

/**
 * Convenience routine for converting byte array into a base64
 * encoded string.
 *
 * @param vp Pointer to variable to convert.
 * @param vp_len Number of bytes to convert.
 * @param buf Buffer where string value will be stored.
 * @param buf_len Size of the buffer.
 *
 * @return 0 on success, non-zero on failure.
 */
char *conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len);

#define CONF_VALUE_SET(str, type, val)                                  \
	conf_value_from_str((str), (type), &(val), sizeof(val))

/**
 * @} sys_config
 */

/*
 * Config storage
 */
struct conf_store_itf;
struct conf_store {
	sys_snode_t cs_next;
	const struct conf_store_itf *cs_itf;
};

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_H_ */
