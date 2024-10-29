// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"

static inline int check_param_types(flamingo_t* flamingo, TSNode params) {
	size_t const n = ts_node_named_child_count(params);

	for (size_t i = 0; i < n; i++) {
		TSNode const child = ts_node_named_child(params, i);
		char const* const child_type = ts_node_type(child);

		if (strcmp(child_type, "param") != 0) {
			return error(flamingo, "expected param in parameter list, got %s", child_type);
		}
	}

	return 0;
}
