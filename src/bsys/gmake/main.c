// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <logging.h>
#include <ncpu.h>
#include <str.h>

#include <stdbool.h>
#include <unistd.h>

// Assuming and following the GNU Standard Makefile conventions:
// https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html

// TODO:
// - Check if regular make command on system is GNU make and use that if gmake doesn't exist.
// - Hook into cleaning, since gmake artifacts are not in bsys_out_path.
// - TODO DESTDIR or PREFIX? ZSTD claims to follow the standard conventions but DESTDIR doesn't work.

#define BUILD_PATH "Makefile"

static bool identify(void) {
	return access(BUILD_PATH, F_OK) != -1;
}

static int setup(void) {
	if (!cmd_exists("gmake")) {
		LOG_FATAL("Couldn't find 'gmake' executable in PATH. GNU Make is something you must install separately.");
		return -1;
	}

	return 0;
}

static int build(void) {
	LOG_INFO("Run GNU Make...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "gmake", NULL);
	cmd_addf(&cmd, "-j%zu", ncpu());
	cmd_set_redirect(&cmd, false, false);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("GNU Make build failed.");
		return -1;
	}

	else {
		LOG_SUCCESS("GNU Make build succeeded.");
	}

	return 0;
}

static int install(void) {
	LOG_SUCCESS("GNU Make install...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "gmake", NULL);
	cmd_addf(&cmd, "PREFIX=%s", install_prefix);
	cmd_add(&cmd, "install");
	cmd_set_redirect(&cmd, false, false);

	cmd_print(&cmd);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("GNU Make install failed.");
		return -1;
	}

	else {
		LOG_SUCCESS("GNU Make install succeeded.");
	}

	return 0;
}

bsys_t const BSYS_GMAKE = {
	.name = "Make (GNU)",
	.key = "gmake",
	.identify = identify,
	.setup = setup,
	.build = build,
	.install = install,
};
