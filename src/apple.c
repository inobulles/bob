// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <cmd.h>

#include <string.h>

int apple_set_install_id(char const* path, char const* id, char** err) {
	cmd_t cmd;
	cmd_create(&cmd, "install_name_tool", path, "-id", NULL);
	cmd_addf(&cmd, "@rpath/%s", id);

	int const rv = cmd_exec(&cmd);

	if (rv < 0 && err != NULL) {
		*err = strdup(cmd_read_out(&cmd));
		size_t const len = strlen(*err);

		if (len >= 1) {
			(*err)[len - 1] = '\0';
		}
	}

	cmd_free(&cmd);
	return rv;
}
