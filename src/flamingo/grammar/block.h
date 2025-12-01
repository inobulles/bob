// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "statement.h"

#include "../common.h"
#include "../env.h"

static int parse_block(flamingo_t* flamingo, TSNode node, flamingo_scope_t** inner_scope) {
	assert(strcmp(ts_node_type(node), "block") == 0);

	env_push_scope(flamingo->env);
	size_t const n = ts_node_named_child_count(node);

	for (size_t i = 0; i < n; i++) {
		TSNode const child = ts_node_named_child(node, i);

		if (parse_statement(flamingo, child) < 0) {
			return -1;
		}
	}

	if (inner_scope == NULL) {
		env_pop_scope(flamingo->env);
	}

	else {
		*inner_scope = env_gently_detach_scope(flamingo->env);
	}

	return 0;
}
