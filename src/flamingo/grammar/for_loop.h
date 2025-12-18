// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2025 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../grammar/expr.h"
#include "../val.h"

static int parse_for_loop(flamingo_t* flamingo, TSNode node) {
	// Get current variable name.

	TSNode const cur_var_name_node = ts_node_child_by_field_name(node, "cur_var_name", 12);
	char const* const cur_var_name_type = ts_node_type(cur_var_name_node);

	if (strcmp(cur_var_name_type, "identifier") != 0) {
		return error(flamingo, "expected identifier for current variable name, got %s", cur_var_name_type);
	}

	size_t const cur_var_name_start = ts_node_start_byte(cur_var_name_node);
	size_t const cur_var_name_end = ts_node_end_byte(cur_var_name_node);

	char const* const cur_var_name = flamingo->src + cur_var_name_start;
	size_t const cur_var_name_size = cur_var_name_end - cur_var_name_start;

	// Get iterator.

	TSNode const iterator_node = ts_node_child_by_field_name(node, "iterator", 8);
	char const* const iterator_type = ts_node_type(iterator_node);

	if (strcmp(iterator_type, "expression") != 0) {
		return error(flamingo, "expected expression for iterator, got %s", iterator_type);
	}

	// Get for body.

	TSNode const body_node = ts_node_child_by_field_name(node, "body", 4);
	char const* const body_type = ts_node_type(body_node);

	if (strcmp(body_type, "block") != 0) {
		return error(flamingo, "expected block for body, got %s", body_type);
	}

	// Evaluate iterator.
	// TODO We might want a mechanism for properly iterating over an iterable, because here we have to evaluate the entire value, which is very inefficient for a range function e.g. (see range vs xrange in Python 2).

	flamingo_val_t* iterator = NULL;

	if (parse_expr(flamingo, iterator_node, &iterator, NULL) < 0) {
		return -1;
	}

	// Get iterator count.
	// This is sort of cheating but eh, see above comment, this is fine for now.

	size_t count;
	flamingo_val_t** elems = NULL;

	switch (iterator->kind) {
	case FLAMINGO_VAL_KIND_VEC:
		count = iterator->vec.count;
		elems = iterator->vec.elems;
		break;
	case FLAMINGO_VAL_KIND_MAP:
		count = iterator->map.count;
		elems = iterator->map.keys;
		break;
	default:
		return error(flamingo, "expected vector or map for iterable, got %s", val_type_str(iterator));
	}

	assert(elems != NULL);

	// Run for loop.

	flamingo->in_loop++;
	flamingo->breaking = false;
	flamingo->continuing = false;

	for (size_t i = 0; i < count; i++) {
		flamingo_val_t* const elem = elems[i];

		// Create scope.

		flamingo_scope_t* const scope = env_push_scope(flamingo->env);

		// Create current variable.
		// Don't need to check if identifier is already in current scope as we're going to add a scope to the stack anyway (which will shadow any previous identifiers with the same name).

		flamingo_var_t* const cur_var = scope_add_var(scope, cur_var_name, cur_var_name_size);
		val_incref(elem);
		cur_var->val = elem;

		// Parse body.
		// TODO It would be nice if parse_block didn't create a new scope for us.
		//      That way, the body can't shadow the current variable.

		if (parse_block(flamingo, body_node, NULL) < 0) {
			return -1;
		}

		// Unwind.

		env_pop_scope(flamingo->env);

		// Check that we're breaking and reset loop state otherwise.

		if (flamingo->breaking) {
			break;
		}

		flamingo->continuing = false;
	}

	// Housekeeping.

	flamingo->in_loop--;
	val_decref(iterator);

	return 0;
}
