// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2025 Aymeric Wibo

#pragma once

#include "../common.h"

static int parse_break(flamingo_t* flamingo) {
	if (!flamingo->in_loop) {
		return error(flamingo, "trying to break outside of a loop");
	}

	assert(!flamingo->breaking);
	flamingo->breaking = true;

	return 0;
}
