// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

bsys_t const* bsys_identify(void) {
	for (size_t i = 0; i < sizeof(BSYS) / sizeof(BSYS[0]); i++) {
		if (BSYS[i]->identify()) {
			return BSYS[i];
		}
	}

	return NULL;
}

int bsys_deps(bsys_t const* bsys) {
	if (bsys->deps == NULL) {
		return 0;
	}

	return bsys->deps();
}

int bsys_build(bsys_t const* bsys, char const* preinstall_prefix) {
	if (bsys->build == NULL) {
		LOG_WARN("%s build system does not have a build step; nothing to build!", bsys->name);
		return 0;
	}

	// Install dependencies.

	if (bsys_deps(bsys) < 0) {
		return -1;
	}

	// Ensure the output path exists.
	// TODO Do this with mkdir_recursive? Do this in main.c?

	if (bsys->key != NULL) {
		char* CLEANUP_STR path;
		asprintf(&path, "%s/%s", out_path, bsys->key);
		assert(path != NULL);

		if (mkdir_wrapped(path, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", path, strerror(errno));
			return -1;
		}

		set_owner(path);
	}

	// Actually build.

	return bsys->build(preinstall_prefix);
}

static void prepend_env(char const* name, char const* fmt, ...) {
	va_list args;

	va_start(args, fmt);

	char* CLEANUP_STR val;
	vasprintf(&val, fmt, args);
	assert(val != NULL);

	va_end(args);

	char* const prev = getenv(name);
	int rv = -1;

	if (prev == NULL) {
		rv = setenv(name, val, 1);
	}

	else {
		// We prepend because earlier entries searched first.

		char* CLEANUP_STR new;
		asprintf(&new, "%s:%s", val, prev);
		assert(new != NULL);

		rv = setenv(name, new, 1);
	}

	if (rv < 0) {
		LOG_WARN("setenv(\"%s\", \"%s\"): %s", name, val, strerror(errno));
	}
}

static int install(bsys_t const* bsys, bool to_prefix) {
	if (bsys->install == NULL) {
		LOG_WARN("%s: build system does not have an install step; nothing to install!", bsys->name);
		return 0;
	}

	// Ensure the output path exists.

	char* CLEANUP_STR path;

	if (to_prefix) {
		asprintf(&path, "%s/prefix", out_path);
	}

	else {
		asprintf(&path, "%s", install_prefix);
	}

	assert(path != NULL);

	if (to_prefix && mkdir_wrapped(path, 0755) < 0 && errno != EEXIST) {
		LOG_FATAL("mkdir(\"%s\"): %s", path, strerror(errno));
		return -1;
	}

	// Run build step.

	if (bsys_build(bsys, path) < 0) {
		return -1;
	}

	// Actually install.

	return bsys->install(path);
}

static void setup_environment(void) {
	prepend_env("PATH", "%s/prefix/bin", out_path);

	prepend_env(
#if defined(__APPLE__)
		"DY" // dyld(1) doesn't recognize `LD_LIBRARY_PATH`.
#endif
		"LD_LIBRARY_PATH",
		"%s/prefix/lib",
		out_path
	);
}

int bsys_run(bsys_t const* bsys, int argc, char* argv[]) {
	if (install(bsys, true) < 0) {
		return -1;
	}

	if (bsys->run == NULL) {
		LOG_WARN("%s: build system does not have a run step; nothing to run!", bsys->name);
		return 0;
	}

	setup_environment();

	return bsys->run(argc, argv);
}

int bsys_sh(bsys_t const* bsys, int argc, char* argv[]) {
	if (install(bsys, true) < 0) {
		return -1;
	}

	setup_environment();

	// Actually run command.

	cmd_t __attribute__((cleanup(cmd_free))) cmd = {0};
	cmd_create(&cmd, NULL);

	if (argc == 0) {
		char const* const shell = getenv("SHELL");

		if (shell == NULL) {
			LOG_FATAL("No command passed to 'bob sh' and $SHELL not set.");
			return -1;
		}

		LOG_SUCCESS("Entering shell ('%s').", shell);
		cmd_add(&cmd, shell);
	}

	else {
		cmd_add_argv(&cmd, argc, argv);
	}

	if (cmd_exec_inplace(&cmd) < 0) {
		LOG_ERROR("Failed to run command.");
		return -1;
	}

	assert(false); // This should never be reached if 'cmd_exec_inplace' succeeds.
}

int bsys_install(bsys_t const* bsys) {
	return install(bsys, false);
}
