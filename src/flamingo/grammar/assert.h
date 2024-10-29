// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../val.h"

static int parse_assert(flamingo_t* flamingo, TSNode node) {
	assert(ts_node_child_count(node) == 2);

	TSNode const test_node = ts_node_child_by_field_name(node, "test", 4);
	char const* const type = ts_node_type(test_node);

	if (strcmp(ts_node_type(test_node), "expression") != 0) {
		return error(flamingo, "expected expression for test, got %s", type);
	}

	flamingo_val_t* val = NULL;

	if (parse_expr(flamingo, test_node, &val, NULL) < 0) {
		return -1;
	}

	if (val->kind != FLAMINGO_VAL_KIND_BOOL) {
		return error(flamingo, "expected boolean value for test, got %s", val_type_str(val));
	}

	size_t const start = ts_node_start_byte(test_node);
	size_t const end = ts_node_end_byte(test_node);

	char const* const test_str = flamingo->src + start;
	size_t const test_size = end - start;

	if (!val->boolean.boolean) {
		return error(flamingo, "assertion test '%.*s' failed", (int) test_size, test_str);
	}

	val_decref(val);
	return 0;
}
