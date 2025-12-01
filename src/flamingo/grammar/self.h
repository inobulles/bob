// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2025 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../env.h"
#include "../val.h"

static int parse_self(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "self") == 0);
	assert(ts_node_named_child_count(node) == 2);

	flamingo_var_t* const var = env_find_var(flamingo->env, "self", 4);

	if (var == NULL) {
		return error(flamingo, "could not find self - are you in a class instance's scope?");
	}

	*val = var->val;
	val_incref(*val);

	return 0;
}
