/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "syscfg/syscfg.h"
#include "config/config.h"
#include "config_priv.h"

#if MYNEWT_VAL(CONFIG_CLI)
#include <string.h>

#include <shell/shell.h>
#include <console/console.h>

static int shell_conf_command(int argc, char **argv);

static struct shell_cmd shell_conf_cmd = {
	.sc_cmd = "config",
	.sc_cmd_func = shell_conf_command
};

static void conf_running_one(char *name, char *val)
{
	console_printf("%s = %s\n", name, val ? val : "<del>");
}

static void conf_dump_running(void)
{
	struct conf_handler *ch;

	SLIST_FOREACH(ch, &conf_handlers, ch_list) {
		if (ch->ch_export) {
			ch->ch_export(conf_running_one, CONF_EXPORT_SHOW);
		}
	}
}

static void conf_saved_one(char *name, char *val, void *cb_arg)
{
	console_printf("%s = %s\n", name, val ? val : "<del>");
}

static void conf_dump_saved(void)
{
	struct conf_store *cs;

	SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
		cs->cs_itf->csi_load(cs, conf_saved_one, NULL);
	}
}

static int shell_conf_command(int argc, char **argv)
{
	char *name = NULL;
	char *val = NULL;
	char tmp_buf[CONF_MAX_VAL_LEN + 1];
	int rc;

	switch (argc) {
	case 2:
		name = argv[1];
		break;
	case 3:
		name = argv[1];
		val = argv[2];
		break;
	default:
		goto err;
	}

	if (!strcmp(name, "commit")) {
		rc = conf_commit(val);
		if (rc) {
			val = "Failed to commit\n";
		} else {
			val = "Done\n";
		}
		console_printf("%s", val);
		return 0;
	} else if (!strcmp(name, "dump")) {
		if (!val || !strcmp(val, "running")) {
			conf_dump_running();
		}
		if (val && !strcmp(val, "saved")) {
			conf_dump_saved();
		}
		return 0;
	} else if (!strcmp(name, "save")) {
		conf_save();
		return 0;
	}
	if (!val) {
		val = conf_get_value(name, tmp_buf, sizeof(tmp_buf));
		if (!val) {
			console_printf("Cannot display value\n");
			goto err;
		}
		console_printf("%s\n", val);
	} else {
		rc = conf_set_value(name, val);
		if (rc) {
			console_printf("Failed to set\n");
			goto err;
		}
	}
	return 0;
err:
	console_printf("Invalid args\n");
	return 0;
}

int conf_cli_register(void)
{
	return shell_cmd_register(&shell_conf_cmd);
}
#endif

