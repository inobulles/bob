// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../val.h"

static int parse_unary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "unary_expression") == 0);
	assert(ts_node_named_child_count(node) == 2);

	// Get operand.

	TSNode const operand = ts_node_child_by_field_name(node, "operand", 7);
	char const* const operand_type = ts_node_type(operand);

	if (strcmp(operand_type, "expression") != 0) {
		return error(flamingo, "expected expression for operand, got %s", operand_type);
	}

	// Get operator.
	// XXX Calling this all 'op_*' because clang-format thinks 'operator' is the C++ keyword and so is annoying with it.

	TSNode const op_node = ts_node_child_by_field_name(node, "operator", 8);
	char const* const op_type = ts_node_type(op_node);

	if (strcmp(op_type, "unary_operator") != 0) {
		return error(flamingo, "expected unary_operator, got %s", op_type);
	}

	size_t const start = ts_node_start_byte(op_node);
	size_t const end = ts_node_end_byte(op_node);

	char const* const op = flamingo->src + start;
	size_t const op_size = end - start;

	// Parse operands.

	flamingo_val_t* operand_val = NULL;

	if (parse_expr(flamingo, operand, &operand_val, NULL) != 0) {
		return -1;
	}

	flamingo_val_kind_t const kind = operand_val->kind;

	// Allocate our value.
	// We can stop here if we discard the result, as we've already evaluated our operand expressions (which we need to do as they might modify state).

	if (val == NULL) {
		goto done;
	}

	assert(*val == NULL);
	*val = val_alloc();

	// Do the math.

	if (kind == FLAMINGO_VAL_KIND_INT) {
		if (strncmp(op, "-", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = -operand_val->integer.integer;
			goto done;
		}
	}

	if (kind == FLAMINGO_VAL_KIND_BOOL) {
		if (strncmp(op, "!", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = !operand_val->boolean.boolean;
			goto done;
		}
	}

	// XXX We don't actually need to decref if there's an error, as the flamingo engine will anyway be entirely freed.
	//     This is robust w.r.t. failures in imported flamingo engines, since we fail if the imported program fails (so the scope is freed instantly).

	return error(flamingo, "unknown operator '%.*s' for type %s", (int) op_size, op, val_type_str(operand_val));

done:

	val_decref(operand_val);

	return 0;
}
