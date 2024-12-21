// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../val.h"

static int parse_assert(flamingo_t* flamingo, TSNode node) {
	size_t const child_count = ts_node_named_child_count(node);
	assert(child_count == 1 || child_count == 2);

	// Get test expression.

	TSNode const test_node = ts_node_child_by_field_name(node, "test", 4);
	char const* const type = ts_node_type(test_node);

	if (strcmp(ts_node_type(test_node), "expression") != 0) {
		return error(flamingo, "expected expression for test, got %s", type);
	}

	// Get message.

	TSNode const msg_node = ts_node_child_by_field_name(node, "msg", 3);
	bool const has_msg = !ts_node_is_null(msg_node);

	if (has_msg) {
		char const* const msg_type = ts_node_type(msg_node);

		if (strcmp(msg_type, "expression") != 0) {
			return error(flamingo, "expected expression for message, got %s", msg_type);
		}
	}

	// Evaluate the test expression.

	flamingo_val_t* val = NULL;

	if (parse_expr(flamingo, test_node, &val, NULL) < 0) {
		return -1;
	}

	if (val->kind != FLAMINGO_VAL_KIND_BOOL) {
		return error(flamingo, "expected boolean value for test, got %s", val_type_str(val));
	}

	// If the test succeeded, exit now.

	if (val->boolean.boolean) {
		val_decref(val);
		return 0;
	}

	// Otherwise, start by getting a string for the test expression.

	size_t const start = ts_node_start_byte(test_node);
	size_t const end = ts_node_end_byte(test_node);

	char const* const test_str = flamingo->src + start;
	size_t const test_size = end - start;

	// If we have a message, evaluate it and include it in the error message.

	if (has_msg) {
		flamingo_val_t* msg_val = NULL;

		if (parse_expr(flamingo, msg_node, &msg_val, NULL) < 0) {
			val_decref(val);
			return error(flamingo, "assertion test '%.*s' failed, and failed to parse message expression in doing so", (int) test_size, test_str);
		}

		char* msg_repr = NULL;

		if (repr(flamingo, msg_val, &msg_repr) < 0) {
			val_decref(val);
			val_decref(msg_val);

			return error(flamingo, "assertion test '%.*s' failed, and failed to represent message expression in doing so", (int) test_size, test_str);
		}

		val_decref(val);
		val_decref(msg_val);

		// Finally, actually error.

		int const rv = error(flamingo, "assertion test '%.*s' failed: %s", (int) test_size, test_str, msg_repr);
		free(msg_repr);

		return rv;
	}

	// Otherwise, just spit out something generic.

	return error(flamingo, "assertion test '%.*s' failed", (int) test_size, test_str);
}
