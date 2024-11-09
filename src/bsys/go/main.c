// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>

#include <stdbool.h>

static bool identify(void) {
	return false;
}

static int build(char const* preinstall_prefix) {
	return 0;
}

bsys_t const BSYS_GO = {
	.name = "Go",
	.identify = identify,
	.build = build,
};
