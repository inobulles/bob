// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../env.h"
#include "../scope.h"
#include "../val.h"
#include "../var.h"
#include "params.h"
#include "static.h"

static int parse_function_declaration(flamingo_t* flamingo, TSNode node, flamingo_fn_kind_t kind) {
	size_t const child_count = ts_node_child_count(node);
	assert(child_count >= 4 || child_count <= 6);

	char const* thing = "unknown";

	switch (kind) {
	case FLAMINGO_FN_KIND_FUNCTION:
		thing = "function";
		break;
	case FLAMINGO_FN_KIND_CLASS:
		thing = "class";
		break;
	case FLAMINGO_FN_KIND_EXTERN:
		thing = "external prototype";
		break;
	default:
		assert(false);
	}

	// Get qualifier list.

	TSNode const qualifiers_node = ts_node_child_by_field_name(node, "qualifiers", 10);
	bool const has_qualifiers = !ts_node_is_null(qualifiers_node);

	if (has_qualifiers) {
		char const* const qualifiers_type = ts_node_type(qualifiers_node);

		if (strcmp(qualifiers_type, "qualifier_list") != 0) {
			return error(flamingo, "expected qualifier_list for qualifiers, got %s", qualifiers_type);
		}
	}

	// Get function/class name.

	TSNode const name_node = ts_node_child_by_field_name(node, "name", 4);
	char const* const name_type = ts_node_type(name_node);

	if (strcmp(name_type, "identifier") != 0) {
		return error(flamingo, "expected identifier for %s name, got %s", thing, name_type);
	}

	size_t const start = ts_node_start_byte(name_node);
	size_t const end = ts_node_end_byte(name_node);

	char const* const name = flamingo->src + start;
	size_t const size = end - start;

	// Get function/class parameters.

	TSNode const params = ts_node_child_by_field_name(node, "params", 6);
	bool const has_params = !ts_node_is_null(params);

	if (has_params) {
		char const* const params_type = ts_node_type(params);

		if (strcmp(params_type, "param_list") != 0) {
			return error(flamingo, "expected param_list for parameters, got %s", params_type);
		}

		if (check_param_types(flamingo, params) < 0) {
			return -1;
		}
	}

	// Get function/class body (only for non-prototypes).

	TSNode body;

	if (kind != FLAMINGO_FN_KIND_EXTERN) {
		body = ts_node_child_by_field_name(node, "body", 4);
		char const* const body_type = ts_node_type(body);

		if (strcmp(body_type, "block") != 0) {
			return error(flamingo, "expected block for body, got %s", body_type);
		}
	}

	// Check if identifier is already in current scope (shallow search) and error if it is.
	// Redeclaring functions/classes is not allowed (I have decided against prototypes).
	// If its in a previous one, that's alright, we'll just shadow it.

	flamingo_scope_t* const cur_scope = env_cur_scope(flamingo->env);
	flamingo_var_t* const prev_var = scope_shallow_find_var(cur_scope, name, size);

	if (prev_var != NULL) {
		return error(flamingo, "the %s '%.*s' has already been declared in this scope", val_role_str(prev_var->val), (int) size, name);
	}

	// Add function/class to scope.

	flamingo_var_t* const var = scope_add_var(cur_scope, name, size);
	var->is_static = check_is_static(flamingo, node);

	var_set_val(var, val_alloc());

	var->val->kind = FLAMINGO_VAL_KIND_FN;
	var->val->owner = cur_scope;

	var->val->fn.kind = kind;
	var->val->fn.env = env_close_over(flamingo->env);
	var->val->fn.params = NULL;

	if (has_params) {
		var->val->fn.params = malloc(sizeof params);
		assert(var->val->fn.params != NULL);
		memcpy(var->val->fn.params, &params, sizeof params);
	}

	// Assign body node.
	// Prototypes by definition don't have bodies.

	if (kind != FLAMINGO_FN_KIND_EXTERN) {
		var->val->fn.body = malloc(sizeof body);
		assert(var->val->fn.body != NULL);
		memcpy(var->val->fn.body, &body, sizeof body);
	}

	// If class, create static environment and look for any static members.

	if (kind == FLAMINGO_FN_KIND_CLASS) {
		flamingo_scope_t* const scope = scope_alloc();

		var->val->fn.scope = scope;
		scope->owner = var->val;

		if (find_static_members_in_class(flamingo, scope, body) < 0) {
			return -1;
		}
	}

	// Since I want 'flamingo.h' to be usable without importing all of Tree-sitter, 'var->val->fn.body' can't just be a 'TSNode'.
	// Thus, since only this file knows about the size of 'TSNode', we must dynamically allocate this on the heap.

	var->val->fn.src = flamingo->src;
	var->val->fn.src_size = flamingo->src_size;

	// If class, once everything is done, call the class declaration callback if one was registered.

	if (kind == FLAMINGO_FN_KIND_CLASS && flamingo->class_decl_cb) {
		return flamingo->class_decl_cb(flamingo, var->val, flamingo->class_decl_cb_data);
	}

	return 0;
}
