// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>

#include <stdbool.h>
#include <unistd.h>

static bool identify(void) {
	if (access("build.fl", F_OK) != -1) {
		return true;
	}

	return false;
}

static int build(void) {
	return 0;
}

bsys_t const BSYS_BOB = {
	.name = "Bob",
	.identify = identify,
	.build = build,
};
