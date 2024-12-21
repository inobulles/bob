// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>

static bool identify(void) {
	return false;
}

static int build(void) {
	return 0;
}

// For 'lsp': -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

bsys_t const BSYS_CMAKE = {
	.name = "CMake",
	.identify = identify,
	.build = build,
};
