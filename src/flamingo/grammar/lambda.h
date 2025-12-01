// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../env.h"
#include "../val.h"

#include "params.h"

static int parse_lambda(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "lambda") == 0);

	// Get parameters.

	TSNode const params = ts_node_child_by_field_name(node, "params", 6);
	bool const has_params = !ts_node_is_null(params);

	if (has_params) {
		char const* const params_type = ts_node_type(params);

		if (strcmp(params_type, "param_list") != 0) {
			return error(flamingo, "expected param_list for anonymous function parameters, got %s", params_type);
		}

		if (check_param_types(flamingo, params) < 0) {
			return -1;
		}
	}

	// Get body (block or expression).

	TSNode const body = ts_node_child_by_field_name(node, "body", 4);
	char const* const body_type = ts_node_type(body);

	bool const is_block = strcmp(body_type, "block") == 0;
	bool const is_expr = strcmp(body_type, "expression") == 0;

	if (!is_block && !is_expr) {
		return error(flamingo, "expected block or expression for anonymous function body, got %s", body_type);
	}

	// Create lambda value.

	if (val == NULL) {
		return 0;
	}

	assert(*val == NULL);
	*val = val_alloc();

	(*val)->kind = FLAMINGO_VAL_KIND_FN;
	(*val)->fn.kind = FLAMINGO_FN_KIND_FUNCTION;
	(*val)->fn.env = env_close_over(flamingo->env);

	(*val)->fn.body = malloc(sizeof body);
	assert((*val)->fn.body != NULL);
	memcpy((*val)->fn.body, &body, sizeof body);

	(*val)->fn.params = NULL;

	if (has_params) {
		(*val)->fn.params = malloc(sizeof params);
		assert((*val)->fn.params != NULL);
		memcpy((*val)->fn.params, &params, sizeof params);
	}

	(*val)->fn.src = flamingo->src;
	(*val)->fn.src_size = flamingo->src_size;

	return 0;
}
