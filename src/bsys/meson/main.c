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

	char* STR_CLEANUP prefix = NULL;
	asprintf(&prefix, "-Dprefix=%s", install_prefix);
	assert(prefix != NULL);

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "meson", "setup", meson_path, prefix, NULL);
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

static int install(void) {
	char* STR_CLEANUP meson_path = NULL;
	asprintf(&meson_path, "%s/meson", out_path);
	assert(meson_path != NULL);

	LOG_SUCCESS("Ninja install...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "ninja", "-C", meson_path, "install", NULL);
	cmd_set_redirect(&cmd, false);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("Ninja install failed.");
		return -1;
	}

	else {
		LOG_SUCCESS("Ninja install succeeded.");
	}

	return 0;
}

bsys_t const BSYS_MESON = {
	.name = "Meson",
	.key = "meson",
	.identify = identify,
	.build = build,
	.install = install,
};
