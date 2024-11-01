// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>
#include <common.h>
#include <logging.h>

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
		return 0;
	}

	// Ensure the output path exists.

	if (bsys->key != NULL) {
		char* CLEANUP_STR path;
		asprintf(&path, "%s/%s", out_path, bsys->key);

		if (mkdir(path, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", path, strerror(errno));
			return -1;
		}
	}

	// Actually build.

	return bsys->build();
}
