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

bsys_t const BSYS_CARGO = {
	.name = "Cargo (Rust)",
	.identify = identify,
	.build = build,
};
