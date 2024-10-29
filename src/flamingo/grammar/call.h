// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../call.h"
#include "../common.h"
#include "../scope.h"

#include "block.h"
#include "expr.h"

// XXX Right now, there's no way of defining primitive type member parameters, so this is for that.
// So I'm just ignoring them for now.
// We need to find a long-term solution at some point.

static int setup_args_no_param(flamingo_t* flamingo, TSNode args) {
	size_t const n = ts_node_named_child_count(args);

	for (size_t i = 0; i < n; i++) {
		// Get argument.

		TSNode const arg = ts_node_named_child(args, i);
		char const* const arg_type = ts_node_type(arg);

		if (strcmp(arg_type, "expression") != 0) {
			return error(flamingo, "expected expression in argument list, got %s", arg_type);
		}

		// Create parameter variable.

		flamingo_scope_t* const scope = env_cur_scope(flamingo->env);
		flamingo_var_t* const var = scope_add_var(scope, "nothing to see here!", 0);

		// Parse argument expression into that parameter variable.

		if (parse_expr(flamingo, arg, &var->val, NULL) < 0) {
			return -1;
		}
	}

	return 0;
}

static int setup_args(flamingo_t* flamingo, TSNode args, TSNode* params) {
	size_t const n = ts_node_named_child_count(args);
	size_t const param_count = params == NULL ? 0 : ts_node_named_child_count(*params);

	if (n != param_count) {
		return error(flamingo, "callable expected %zu arguments, got %zu instead", param_count, n);
	}

	flamingo_scope_t* const scope = env_cur_scope(flamingo->env);

	for (size_t i = 0; i < n; i++) {
		// Get argument.

		TSNode const arg = ts_node_named_child(args, i);
		char const* const arg_type = ts_node_type(arg);

		if (strcmp(arg_type, "expression") != 0) {
			return error(flamingo, "expected expression in argument list, got %s", arg_type);
		}

		// Get parameter in same position.
		// assert: Type should already have been checked when declaring the function/class.

		TSNode const param = ts_node_named_child(*params, i);
		char const* const param_type = ts_node_type(param);
		assert(strcmp(param_type, "param") == 0);

		// Get parameter identifier.

		TSNode const identifier = ts_node_child_by_field_name(param, "ident", 5);
		assert(strcmp(ts_node_type(identifier), "identifier") == 0);

		size_t const start = ts_node_start_byte(identifier);
		size_t const end = ts_node_end_byte(identifier);

		char const* const name = flamingo->src + start;
		size_t const size = end - start;

		// Get parameter type if it has one.

		TSNode const type = ts_node_child_by_field_name(param, "type", 4);
		bool const has_type = !ts_node_is_null(type);

		if (has_type) {
			assert(strcmp(ts_node_type(type), "type") == 0);
		}

		(void) type;

		// Create parameter variable.

		flamingo_var_t* const var = scope_add_var(scope, name, size);

		// Parse argument expression into that parameter variable.

		if (parse_expr(flamingo, arg, &var->val, NULL) < 0) {
			return -1;
		}
	}

	return 0;
}

typedef struct {
	flamingo_val_t* callable;
	bool has_args;
	TSNode args;
} set_args_data_t;

static int set_args_cb(flamingo_t* flamingo, void* _data) {
	set_args_data_t* const data = _data;

	// Evaluate argument expressions.
	// Add our arguments as variables, with the function parameters as names.

	bool const is_ptm = data->callable->fn.kind == FLAMINGO_FN_KIND_PTM;
	TSNode* const params = data->callable->fn.params;

	if (data->has_args) {
		if (is_ptm) {
			if (setup_args_no_param(flamingo, data->args) < 0) {
				return -1;
			}
		}

		else if (setup_args(flamingo, data->args, params) < 0) {
			return -1;
		}
	}

	return 0;
}

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

	// Actually call.

	set_args_data_t data = {
		.callable = callable,
		.has_args = has_args,
		.args = args,
	};

	return call_with_set_args_cb(flamingo, callable, accessed_val, val, set_args_cb, &data);
}
