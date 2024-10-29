// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../env.h"
#include "../scope.h"

static int parse_var_decl(flamingo_t* flamingo, TSNode node) {
	size_t const child_count = ts_node_named_child_count(node);
	assert(child_count >= 1 && child_count <= 3);

	// Get variable name.

	TSNode const name_node = ts_node_child_by_field_name(node, "name", 4);
	char const* const name_type = ts_node_type(name_node);

	if (strcmp(name_type, "identifier") != 0) {
		return error(flamingo, "expected identifier for name, got %s", name_type);
	}

	size_t const start = ts_node_start_byte(name_node);
	size_t const end = ts_node_end_byte(name_node);

	char const* const name = flamingo->src + start;
	size_t const name_size = end - start;

	// Get type if there is one.

	TSNode const type_node = ts_node_child_by_field_name(node, "type", 4);
	bool const has_type = !ts_node_is_null(type_node);

	if (has_type) {
		char const* const type_type = ts_node_type(type_node);

		if (strcmp(type_type, "type") != 0) {
			return error(flamingo, "expected type for type, got %s", type_type);
		}
	}

	// Get initial value if there is one.

	TSNode const initial_node = ts_node_child_by_field_name(node, "initial", 7);
	bool const has_initial = !ts_node_is_null(initial_node);

	if (has_initial) {
		char const* const initial_type = ts_node_type(initial_node);

		if (strcmp(initial_type, "expression") != 0) {
			return error(flamingo, "expected expression for initial value, got %s", initial_type);
		}
	}

	// Check if identifier is already in current scope (shallow search) and error if it is.
	// If its in a previous one, that's alright, we'll just shadow it.

	flamingo_scope_t* const cur_scope = env_cur_scope(flamingo->env);
	flamingo_var_t* const prev_var = scope_shallow_find_var(cur_scope, name, name_size);

	if (prev_var != NULL) {
		return error(flamingo, "%s '%.*s' already declared in this scope", val_role_str(prev_var->val), (int) name_size, name);
	}

	// Now, we can add our variable to the scope.

	flamingo_var_t* const var = scope_add_var(cur_scope, name, name_size);

	// And parse the initial expression if there is one to the variable's value.

	if (has_initial) {
		if (parse_expr(flamingo, initial_node, &var->val, NULL) < 0) {
			return -1;
		}
	}

	else {
		var->val = val_alloc();
		var->val->kind = FLAMINGO_VAL_KIND_NONE;
	}

	return 0;
}
