// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

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

static int configure(bool reconfigure) {
	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "meson", "setup", NULL);

	if (reconfigure) {
		cmd_add(&cmd, "--reconfigure");
	}

	cmd_add(&cmd, bsys_out_path);
	cmd_addf(&cmd, "-Dprefix=%s", install_prefix);

	cmd_set_redirect(&cmd, false, false);
	return cmd_exec(&cmd);
}

static int build(void) {
	LOG_INFO("Meson setup...");

	// We don't check for failure here, because if we've already run setup, this will fail saying we don't need to run this again.
	// If there is a real error, it will be caught by the Ninja build anyway.

	configure(false);

	LOG_INFO("Ninja build...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_free(&cmd);
	cmd_create(&cmd, "ninja", "-C", bsys_out_path, NULL);
	cmd_set_redirect(&cmd, false, false);

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
	cmd_set_redirect(&cmd, true, true);

	int rv = cmd_exec(&cmd);

	// If Ninja suggests we reconfigure directory, do that.
	// This can happen e.g. if there's a big Meson version change.

	if (strstr(cmd.out_buf, "Consider reconfiguring the directory") != NULL) {
		LOG_INFO("Try to reconfigure the Meson build directory...");

		if (configure(true) < 0) {
			LOG_FATAL("Meson reconfigure failed.");
			return -1;
		}

		// Retry Ninja install.

		cmd_set_redirect(&cmd, false, false);
		rv = cmd_exec(&cmd);
	}

	if (rv < 0) {
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
