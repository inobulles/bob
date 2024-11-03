// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>
#include <common.h>
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

		if (mkdir(path, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", path, strerror(errno));
			return -1;
		}
	}

	// Actually build.

	return bsys->build();
}

static int install(bsys_t const* bsys, bool to_prefix) {
	assert(to_prefix == true); // TODO Installing to system.

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
	asprintf(&path, "%s/prefix", out_path);
	assert(path != NULL);

	if (to_prefix && mkdir(path, 0755) < 0 && errno != EEXIST) {
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

	return bsys->run(argc, argv);
}

int bsys_install(bsys_t const* bsys) {
	return install(bsys, false);
}
