// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <bsys.h>
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

int bsys_build(bsys_t const* bsys) {
	if (bsys->build == NULL) {
		LOG_WARN("%s build system does not have a build step; nothing to build!", bsys->name);
		return 0;
	}

	// Ensure the output path exists.

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

	return bsys->build();
}

static void append_env(char const* name, char const* fmt, ...) {
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
		char* CLEANUP_STR new;
		asprintf(&new, "%s:%s", prev, val);
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

	// Run build step.

	if (bsys_build(bsys) < 0) {
		return -1;
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

	// Actually install.

	return bsys->install(path);
}

int bsys_run(bsys_t const* bsys, int argc, char* argv[]) {
	if (install(bsys, true) < 0) {
		return -1;
	}

	if (bsys->run == NULL) {
		LOG_WARN("%s: build system does not have a run step; nothing to run!", bsys->name);
		return 0;
	}

	// Set up environment to be inside the prefix.

	append_env("PATH", "%s/prefix/bin", out_path);
	append_env("LD_LIBRARY_PATH", "%s/prefix/lib", out_path);

	// Actually run.

	return bsys->run(argc, argv);
}

int bsys_install(bsys_t const* bsys) {
	return install(bsys, false);
}
