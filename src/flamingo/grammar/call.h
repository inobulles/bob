// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../call.h"
#include "../common.h"

#include "expr.h"

static int parse_call(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "call") == 0);
	assert(ts_node_child_count(node) == 3 || ts_node_child_count(node) == 4);

	// Get callable expression.

	TSNode const callable_node = ts_node_child_by_field_name(node, "callable", 8);
	char const* const callable_type = ts_node_type(callable_node);

	if (strcmp(callable_type, "expression") != 0) {
		return error(flamingo, "expected identifier for callable name, got %s", callable_type);
	}

	// Get arguments.

	TSNode const args = ts_node_child_by_field_name(node, "args", 4);
	bool const has_args = !ts_node_is_null(args);

	if (has_args) {
		char const* const args_type = ts_node_type(args);

		if (strcmp(args_type, "arg_list") != 0) {
			return error(flamingo, "expected arg_list for parameters, got %s", args_type);
		}
	}

	// Evaluate callable expression.

	flamingo_val_t* callable = NULL;
	flamingo_val_t* accessed_val = NULL;

	if (parse_expr(flamingo, callable_node, &callable, &accessed_val) < 0) {
		return -1;
	}

	if (callable->kind != FLAMINGO_VAL_KIND_FN) {
		return error(flamingo, "callable expression is of type %s, which is not callable", val_type_str(callable));
	}

	// Evaluate arguments.

	flamingo_arg_list_t arg_list = {
		.count = 0,
	};

	if (has_args) {
		arg_list.count = ts_node_named_child_count(args);
		flamingo_val_t** const arg_vals = alloca(arg_list.count * sizeof *arg_vals);
		arg_list.args = arg_vals;

		for (size_t i = 0; i < arg_list.count; i++) {
			// Get argument.

			TSNode const arg = ts_node_named_child(args, i);
			char const* const arg_type = ts_node_type(arg);

			if (strcmp(arg_type, "expression") != 0) {
				return error(flamingo, "expected expression in argument list, got %s", arg_type);
			}

			// Evaluate argument expression.

			flamingo_val_t* val = NULL;

			if (parse_expr(flamingo, arg, &val, NULL) < 0) {
				return -1;
			}

			arg_vals[i] = val;
		}
	}

	// Actually call.

	return call(flamingo, callable, accessed_val, val, &arg_list);
}
