// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>

#include <stdbool.h>

static bool identify(void) {
	return false;
}

static int build(void) {
	return 0;
}

bsys_t const BSYS_MAKE_FREEBSD_PORT = {
	.name = "Make (FreeBSD port)",
	.identify = identify,
	.build = build,
};
