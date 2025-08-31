// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2025 Aymeric Wibo

#pragma once

#include "../common.h"

static int parse_continue(flamingo_t* flamingo) {
	if (!flamingo->in_loop) {
		return error(flamingo, "trying to continue outside of a loop");
	}

	assert(!flamingo->continuing);
	flamingo->continuing = true;

	return 0;
}
