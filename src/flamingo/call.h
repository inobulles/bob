// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "common.h"
#include "env.h"
#include "scope.h"
#include "var.h"

#include "grammar/block.h"
#include "grammar/expr.h"

static int call_with_set_args_cb(
	flamingo_t* flamingo,
	flamingo_val_t* callable,
	flamingo_val_t* accessed_val,
	flamingo_val_t** rv,
	set_args_cb_t set_args_cb,
	void* set_args_data
) {
	bool const is_class = callable->fn.kind == FLAMINGO_FN_KIND_CLASS;
	bool const on_inst = accessed_val != NULL && accessed_val->kind == FLAMINGO_VAL_KIND_INST;
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
	flamingo->env = callable->fn.env == NULL ? prev_env : callable->fn.env;

	// If calling on an instance, add that instance's scope to the scope stack.
	// We do this before the arguments scope because we want parameters to shadow stuff in the instance's scope.

	if (on_inst) {
		env_gently_attach_scope(flamingo->env, accessed_val->inst.scope);
	}

	// Create a new scope for the function for the argument assignments.
	// It's important to set 'scope->class_scope' to false for functions as new scopes will copy the 'class_scope' property from their parents otherwise.

	flamingo_scope_t* scope = env_push_scope(flamingo->env);
	scope->class_scope = is_class;

	if (set_args_cb(flamingo, set_args_data) < 0) {
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

		if (is_extern && flamingo->external_fn_cb(flamingo, callable->name_size, callable->name, flamingo->external_fn_cb_data, &arg_list, &flamingo->cur_fn_rv) < 0) {
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

	if (on_inst) {
		env_gently_detach_scope(flamingo->env);
	}

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

typedef struct {
	flamingo_arg_list_t* arg_list;
	TSNode* params;
} set_args_from_arg_list_data_t;

// TODO This is really quite similar to 'setup_args', factor out the common code?

static int set_args_from_arg_list(flamingo_t* flamingo, void* _data) {
	set_args_from_arg_list_data_t* const data = _data;

	size_t n = data->arg_list->count;
	size_t const param_count = data->params == NULL ? 0 : ts_node_named_child_count(*data->params);

	if (n != param_count) {
		return error(flamingo, "callable expected %zu arguments, got %zu instead", param_count, n);
	}

	flamingo_scope_t* const scope = env_cur_scope(flamingo->env);

	for (size_t i = 0; i < n; i++) {
		// Get parameter.
		// assert: Type should already have been checked when declaring the function/class.

		TSNode const param = ts_node_named_child(*data->params, i);
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
		var_set_val(var, data->arg_list->args[i]);
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
	set_args_from_arg_list_data_t set_args_data = {
		.arg_list = args,
		.params = callable->fn.params,
	};

	return call_with_set_args_cb(flamingo, callable, accessed_val, rv, set_args_from_arg_list, &set_args_data);
}
