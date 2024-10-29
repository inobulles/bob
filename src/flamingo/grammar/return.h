// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../env.h"
#include "../val.h"

static int parse_return(flamingo_t* flamingo, TSNode node) {
	assert(ts_node_child_count(node) == 1 || ts_node_child_count(node) == 2);

	// Don't allow returns in top level scopes.
	// Maybe in the future this could serve as an exit kind of thing, but I think I want an explicit 'exit' statement.

	if (env_parent_scope(flamingo->env) == NULL) {
		return error(flamingo, "Return can't be used in top-level scope");
	}

	bool const in_class = env_cur_scope(flamingo->env)->class_scope;

	// Get return value.

	TSNode const rv_node = ts_node_child_by_field_name(node, "rv", 2);
	bool const has_rv = !ts_node_is_null(rv_node);

	if (has_rv) {
		if (in_class) {
			return error(flamingo, "return statement can't take a return value when inside a class scope");
		}

		char const* const type = ts_node_type(rv_node);

		if (strcmp(ts_node_type(rv_node), "expression") != 0) {
			return error(flamingo, "expected expression for return value, got %s", type);
		}
	}

	// Parse the return value expression if there is one (none value otherwise).
	// State of the current function return value should always be clean before this.

	assert(flamingo->cur_fn_rv == NULL);

	if (in_class) {
		flamingo->cur_fn_rv = (void*) (intptr_t) 0xC1A55EE; // XXX Yeaaaah this is a bit ugly but hey.
		return 0;
	}

	if (has_rv) {
		if (parse_expr(flamingo, rv_node, &flamingo->cur_fn_rv, NULL) < 0) {
			return -1;
		}
	}

	else {
		flamingo->cur_fn_rv = val_alloc();
	}

	return 0;
}
