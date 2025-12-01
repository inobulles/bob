// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../env.h"
#include "../val.h"

static int parse_identifier(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "identifier") == 0);
	assert(ts_node_child_count(node) == 0);

	size_t const start = ts_node_start_byte(node);
	size_t const end = ts_node_end_byte(node);

	char const* const identifier = flamingo->src + start;
	size_t const size = end - start;

	flamingo_var_t* const var = env_find_var(flamingo->env, identifier, size);

	if (var == NULL) {
		return error(flamingo, "could not find identifier: %.*s", (int) size, identifier);
	}

	*val = var->val;
	val_incref(*val);

	return 0;
}
