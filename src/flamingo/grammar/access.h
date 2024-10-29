// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../scope.h"

static int access_find_var(flamingo_t* flamingo, TSNode node, flamingo_var_t** var, flamingo_val_t** accessed_val) {
	assert(var != NULL);
	assert(accessed_val != NULL);
	assert(strcmp(ts_node_type(node), "access") == 0);
	assert(ts_node_named_child_count(node) == 2);

	// Get accessed expression.

	TSNode const accessed = ts_node_child_by_field_name(node, "accessed", 8);
	char const* const accessed_type = ts_node_type(accessed);

	if (strcmp(accessed_type, "expression") != 0) {
		return error(flamingo, "expected expression for accessed, got %s", accessed_type);
	}

	// Get accessor identifier.

	TSNode const accessor_node = ts_node_child_by_field_name(node, "accessor", 8);
	char const* const accessor_type = ts_node_type(accessor_node);

	if (strcmp(accessor_type, "identifier") != 0) {
		return error(flamingo, "expected identifier for accessor, got %s", accessor_type);
	}

	size_t const start = ts_node_start_byte(accessor_node);
	size_t const end = ts_node_end_byte(accessor_node);

	char const* const accessor = flamingo->src + start;
	size_t const size = end - start;

	// Parse accessed expression.

	if (parse_expr(flamingo, accessed, accessed_val, NULL) != 0) {
		return -1;
	}

	// Actually access.

	*var = NULL;
	flamingo_val_kind_t const kind = (*accessed_val)->kind;

	bool const is_inst = kind == FLAMINGO_VAL_KIND_INST;
	bool const is_static = kind == FLAMINGO_VAL_KIND_FN && (*accessed_val)->fn.kind == FLAMINGO_FN_KIND_CLASS;

	if (is_inst || is_static) {
		flamingo_scope_t* const scope = is_inst ? (*accessed_val)->inst.scope : (*accessed_val)->fn.scope;
		*var = scope_shallow_find_var(scope, accessor, size);

		if (*var == NULL) {
			return error(flamingo, "member '%.*s' was never in declared", (int) size, accessor);
		}

		return 0;
	}

	// Is PTM access.

	size_t const count = flamingo->primitive_type_members[kind].count;
	flamingo_var_t* const type_vars = flamingo->primitive_type_members[kind].vars;

	for (size_t i = 0; i < count; i++) {
		flamingo_var_t* const type_var = &type_vars[i];

		if (flamingo_strcmp(type_var->key, accessor, type_var->key_size, size) == 0) {
			*var = type_var;
			break;
		}
	}

	if (*var == NULL) {
		return error(flamingo, "primitive type member '%.*s' doesn't exist on expression of type %s", (int) size, accessor, val_type_str(*accessed_val));
	}

	return 0;
}

static int parse_access(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val_ref) {
	flamingo_var_t* var;
	flamingo_val_t* accessed_val = NULL;

	if (access_find_var(flamingo, node, &var, &accessed_val) < 0) {
		return -1;
	}

	if (accessed_val_ref != NULL) {
		*accessed_val_ref = accessed_val;
	}

	// Set value.

	if (val == NULL) {
		return 0;
	}

	assert(*val == NULL);
	*val = var->val;
	val_incref(*val);

	// TODO val_decref(accessed_val);
	return 0;
}
