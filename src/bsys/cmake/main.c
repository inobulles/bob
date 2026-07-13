// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <logging.h>

#include <stdio.h>
#include <unistd.h>

#define BUILD_PATH "CMakeLists.txt"

static bool identify(void) {
	return access(BUILD_PATH, F_OK) != -1;
}

static int setup(void) {
	if (!cmd_exists("cmake")) {
		LOG_FATAL("Couldn't find 'cmake' executable in PATH. CMake is something you must install separately.");
		return -1;
	}

	return 0;
}

static int configure(void) {
	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "cmake", "-S", ".", "-B", bsys_out_path, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", NULL);

	cmd_addf(&cmd, "-DCMAKE_INSTALL_PREFIX=%s", install_prefix);

	for (size_t i = 0; i < dep_config_count; i++) {
		cmd_addf(&cmd, "-D%s=%s", dep_config_keys[i], dep_config_vals[i]);
	}

	if (cmd_exists("ninja")) {
		cmd_add(&cmd, "-GNinja");
	}

	cmd_set_redirect(&cmd, CMD_NO_REDIRECT, CMD_NO_FORCE_REDIRECT);
	return cmd_exec(&cmd);
}

static int build(void) {
	LOG_INFO("CMake configure...");

	// Re-running configure is idempotent; it also picks up CMakeLists.txt changes.

	if (configure() < 0) {
		LOG_FATAL("CMake configure failed.");
		return -1;
	}

	LOG_INFO("CMake build...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "cmake", "--build", bsys_out_path, NULL);
	cmd_set_redirect(&cmd, CMD_NO_REDIRECT, CMD_NO_FORCE_REDIRECT);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("CMake build failed.");
		return -1;
	}

	LOG_SUCCESS("CMake build succeeded.");
	return 0;
}

static int install(void) {
	LOG_INFO("CMake install...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "cmake", "--install", bsys_out_path, NULL);
	cmd_set_redirect(&cmd, CMD_NO_REDIRECT, CMD_NO_FORCE_REDIRECT);

	if (cmd_exec(&cmd) < 0) {
		LOG_FATAL("CMake install failed.");
		return -1;
	}

	LOG_SUCCESS("CMake install succeeded.");
	return 0;
}

bsys_t const BSYS_CMAKE = {
	.name = "CMake",
	.key = "cmake",
	.supports_config = true,
	.identify = identify,
	.setup = setup,
	.build = build,
	.install = install,
};
