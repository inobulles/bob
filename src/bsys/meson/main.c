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

static int setup(void) {
	if (!cmd_exists("meson")) {
		LOG_FATAL("Couldn't find 'meson' executable in PATH. Meson is something you must install separately.");
		return -1;
	}

	if (!cmd_exists("ninja")) {
		LOG_FATAL("Couldn't find 'ninja' executable in PATH. Ninja is something you must install separately, and is necessary for the Meson BSYS.");
		return -1;
	}

	return 0;
}

static int build(void) {
	LOG_INFO("Meson setup...");

	char* STR_CLEANUP prefix = NULL;
	asprintf(&prefix, "-Dprefix=%s", install_prefix);
	assert(prefix != NULL);

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "meson", "setup", bsys_out_path, prefix, NULL);
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
	cmd_create(&cmd, "ninja", "-C", bsys_out_path, NULL);
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
	LOG_SUCCESS("Ninja install...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "ninja", "-C", bsys_out_path, "install", NULL);
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
	.setup = setup,
	.build = build,
	.install = install,
};
