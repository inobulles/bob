// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

/*
 * Function calling.
 *
 * This file implements the logic for calling various types of callables in Flamingo, including functions, classes, external functions, and primitive type members (PTMs).
 *
 * The {@link call} function handles environment switching, argument setup, and execution of the callable's body or callback.
 */

#pragma once

#include "common.h"
#include "env.h"
#include "scope.h"
#include "var.h"

#include "grammar/block.h"
#include "grammar/expr.h"

// XXX Right now, there's no way of defining primitive type member parameters, so this is for that.
// So I'm just ignoring them for now.
// We need to find a long-term solution at some point.

static int setup_args_no_param(flamingo_t* flamingo, flamingo_arg_list_t* args) {
	for (size_t i = 0; i < args->count; i++) {
		// Create parameter variable.

		flamingo_scope_t* const scope = env_cur_scope(flamingo->env);
		flamingo_var_t* const var = scope_add_var(scope, "nothing to see here!", 0);

		var_set_val(var, args->args[i]);
		val_incref(args->args[i]);
	}

	return 0;
}

static int setup_args(flamingo_t* flamingo, TSNode* params, flamingo_arg_list_t* args) {
	size_t const param_count = params == NULL ? 0 : ts_node_named_child_count(*params);

	if (args->count != param_count) {
		return error(flamingo, "callable expected %zu arguments, got %zu instead", param_count, args->count);
	}

	flamingo_scope_t* const scope = env_cur_scope(flamingo->env);

	for (size_t i = 0; i < args->count; i++) {
		// Get parameter.
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

		// Create parameter variable, and set to argument list value in same position.

		flamingo_var_t* const var = scope_add_var(scope, name, size);
		var_set_val(var, args->args[i]);
		val_incref(args->args[i]);
	}

	return 0;
}

static int call(
	flamingo_t* flamingo,
	flamingo_val_t* callable,
	flamingo_val_t* accessed_val,
	flamingo_val_t** rv,
	flamingo_arg_list_t* args
) {
	// Note about calling functions on instances: we don't actually need to add its scope to the environment, because the environment on its callable already has that scope on the scope stack.
	// If we ever need to know whether or not we're calling on an instance, the following check can be used: accessed_val != NULL && accessed_val->kind == FLAMINGO_VAL_KIND_INST

	bool const is_class = callable->fn.kind == FLAMINGO_FN_KIND_CLASS;
	bool const is_extern = callable->fn.kind == FLAMINGO_FN_KIND_EXTERN;
	bool const is_ptm = callable->fn.kind == FLAMINGO_FN_KIND_PTM;

	// Actually call the callable.
	// Switch context's source if the callable was created in another.

	char* const prev_src = flamingo->src;
	size_t const prev_src_size = flamingo->src_size;

	if (callable->fn.src != NULL) {
		flamingo->src = callable->fn.src;
		flamingo->src_size = callable->fn.src_size;
	}

	// Switch context's current callable body if we were called from another.

	TSNode* const prev_fn_body = flamingo->cur_fn_body;
	flamingo->cur_fn_body = callable->fn.body;

	// Switch context's current environment to the one closed over by the function.

	flamingo_env_t* const prev_env = flamingo->env;

	if (callable->fn.env != NULL) {
		flamingo->env = callable->fn.env;
	}

	// Create a new scope for the function for the argument assignments.
	// It's important to set 'scope->class_scope' to false for functions as new scopes will copy the 'class_scope' property from their parents otherwise.

	flamingo_scope_t* const scope = env_push_scope(flamingo->env);
	scope->class_scope = is_class;

	if (is_ptm) {
		if (setup_args_no_param(flamingo, args) < 0) {
			return -1;
		}
	}

	else if (setup_args(flamingo, callable->fn.params, args) < 0) {
		return -1;
	}

	// If external function or primitive type member: call the function's callback.
	// If function or class: actually parse the function's body.

	TSNode* const body = callable->fn.body;
	bool const is_expr = body != NULL && strcmp(ts_node_type(*body), "expression") == 0;

	flamingo_scope_t* inner_scope;

	if (is_extern || is_ptm) {
		if (is_extern && flamingo->external_fn_cb == NULL) {
			return error(flamingo, "cannot call external function without a external function callback being set");
		}

		// Create arg list.

		flamingo_scope_t* const arg_scope = env_cur_scope(flamingo->env);

		size_t const arg_count = arg_scope->vars_size;
		flamingo_val_t** const args = malloc(arg_count * sizeof *args);
		assert(args != NULL);

		for (size_t i = 0; i < arg_count; i++) {
			args[i] = arg_scope->vars[i].val;
		}

		flamingo_arg_list_t arg_list = {
			.count = arg_count,
			.args = args,
		};

		// Actually call the external function callback or primitive type member.

		assert(flamingo->cur_fn_rv == NULL);

		if (is_extern && flamingo->external_fn_cb(flamingo, callable, flamingo->external_fn_cb_data, &arg_list, &flamingo->cur_fn_rv) < 0) {
			return -1;
		}

		else if (is_ptm && callable->fn.ptm_cb(flamingo, accessed_val, &arg_list, &flamingo->cur_fn_rv)) {
			return -1;
		}

		free(args);
	}

	else if (is_expr) {
		assert(callable->fn.kind == FLAMINGO_FN_KIND_FUNCTION); // The only kind of callable that can have an expression body.

		if (parse_expr(flamingo, *body, rv, NULL) < 0) {
			return -1;
		}
	}

	else if (parse_block(flamingo, *body, is_class ? &inner_scope : NULL) < 0) {
		return -1;
	}

	// Unwind the scope stack and switch back to previous source, current function body context, and environment..

	env_pop_scope(flamingo->env);

	flamingo->src = prev_src;
	flamingo->src_size = prev_src_size;

	flamingo->cur_fn_body = prev_fn_body;
	flamingo->env = prev_env;

	// If the function body was an expression, we're done (no return value, call value was already set).

	if (is_expr) {
		goto done;
	}

	// If class, create an instance.

	if (callable->fn.kind == FLAMINGO_FN_KIND_CLASS) {
		if (rv == NULL) {
			goto done;
		}

		assert(*rv == NULL);
		*rv = val_alloc();
		(*rv)->kind = FLAMINGO_VAL_KIND_INST;

		(*rv)->inst.class = callable;
		(*rv)->inst.scope = inner_scope;
		(*rv)->inst.data = NULL;
		(*rv)->inst.free_data = NULL;

		inner_scope->owner = *rv;

		// XXX A small quirk to note regarding this: an instance won't be created if it isn't assigned to anything.
		// That means that the class instantiation callback will never be called either, even if the constructor code itself is.

		if (flamingo->class_inst_cb != NULL && flamingo->class_inst_cb(flamingo, *rv, flamingo->class_inst_cb_data, args) < 0) {
			return -1;
		}

		// Add self variable to inner scope.

		flamingo_var_t* const self = scope_add_var(inner_scope, "self", 4);
		var_set_val(self, *rv);

		goto done;
	}

	// Set the value of this expression to the return value.
	// Just discard it if we're not going to set it to anything.

	if (rv == NULL) {
		if (flamingo->cur_fn_rv != NULL) {
			val_decref(flamingo->cur_fn_rv);
		}
	}

	else {
		// If return value was never set, just create a none value.

		if (flamingo->cur_fn_rv == NULL) {
			flamingo->cur_fn_rv = val_alloc();
		}

		assert(*rv == NULL);
		*rv = flamingo->cur_fn_rv;
	}

done:

	flamingo->cur_fn_rv = NULL;
	return 0;
}
