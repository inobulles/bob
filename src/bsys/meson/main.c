// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#define BUILD_PATH "meson.build"

static bool identify(void) {
	return access(BUILD_PATH, F_OK) != -1;
}

static int build(void) {
	char* STR_CLEANUP meson_path = NULL;
	asprintf(&meson_path, "%s/meson", out_path);
	assert(meson_path != NULL);

	LOG_INFO("Meson setup...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "meson", "setup", meson_path);
	cmd_set_redirect(&cmd, false);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("Meson setup failed.");
		return -1;
	}

	else {
		LOG_SUCCESS("Meson setup succeeded.");
	}

	LOG_INFO("Ninja build...");

	cmd_free(&cmd);
	cmd_create(&cmd, "ninja", "-C", meson_path, NULL);
	cmd_set_redirect(&cmd, false);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("Ninja build failed.");
		return -1;
	}

	else {
		LOG_SUCCESS("Ninja build succeeded.");
	}

	return 0;
}

bsys_t const BSYS_MESON = {
	.name = "Meson",
	.key = "meson",
	.identify = identify,
	.build = build,
};
