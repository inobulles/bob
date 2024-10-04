// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include <common.h>
#include <val.h>

#include <math.h>

static bool equality(char const* op, size_t op_size, flamingo_val_t* left_val, flamingo_val_t* right_val, flamingo_val_t** val) {
	if (strncmp(op, "==", op_size) == 0) {
		(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
		(*val)->boolean.boolean = val_eq(left_val, right_val);
		return true;
	}

	if (strncmp(op, "!=", op_size) == 0) {
		(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
		(*val)->boolean.boolean = !val_eq(left_val, right_val);
		return true;
	}

	return false;
}

static int parse_binary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "binary_expression") == 0);
	assert(ts_node_named_child_count(node) == 3);

	// Get left operand.

	TSNode const left = ts_node_child_by_field_name(node, "left", 4);
	char const* const left_type = ts_node_type(left);

	if (strcmp(left_type, "expression") != 0) {
		return error(flamingo, "expected expression for left operand, got %s", left_type);
	}

	// Get right operand.

	TSNode const right = ts_node_child_by_field_name(node, "right", 5);
	char const* const right_type = ts_node_type(right);

	if (strcmp(right_type, "expression") != 0) {
		return error(flamingo, "expected expression for right operand, got %s", right_type);
	}

	// Get operator.
	// XXX Calling this all 'op_*' because clang-format thinks 'operator' is the C++ keyword and so is annoying with it.

	TSNode const op_node = ts_node_child_by_field_name(node, "operator", 8);
	char const* const op_type = ts_node_type(op_node);

	if (strstr("operator", op_type) != 0) { // XXX Yes, I'm being lazy.
		return error(flamingo, "expected operator, got %s", op_type);
	}

	size_t const start = ts_node_start_byte(op_node);
	size_t const end = ts_node_end_byte(op_node);

	char const* const op = flamingo->src + start;
	size_t const op_size = end - start;

	// Parse operands.

	flamingo_val_t* left_val = NULL;

	if (parse_expr(flamingo, left, &left_val, NULL) != 0) {
		return -1;
	}

	flamingo_val_t* right_val = NULL;

	if (parse_expr(flamingo, right, &right_val, NULL) != 0) {
		return -1;
	}

	// Check if the operands are compatible.

	bool const same_types = left_val->kind == right_val->kind;
	bool const none_comparison = left_val->kind == FLAMINGO_VAL_KIND_NONE || right_val->kind == FLAMINGO_VAL_KIND_NONE;

	if (!same_types && !none_comparison) {
		return error(flamingo, "operands have incompatible types: %s and %s", val_type_str(left_val), val_type_str(right_val));
	}

	flamingo_val_kind_t const kind = left_val->kind; // Same as 'right_val->kind' by this point.

	// Allocate our value.
	// We can stop here if we discard the result, as we've already evaluated our operand expressions (which we need to do as they might modify state).
	// TODO It would be nice to find a way to check the operands are compatible with the operator before this point so we get errors. Though I don't think this is the only place where we're not doing this correctly so this might be a difficult change to make.

	if (val == NULL) {
		return 0;
	}

	assert(*val == NULL);
	*val = val_alloc();

	// Do the math.

	if (none_comparison) {
		(*val)->kind = FLAMINGO_VAL_KIND_BOOL;

		if (strncmp(op, "==", op_size) == 0) {
			(*val)->boolean.boolean = same_types;
			goto done;
		}

		if (strncmp(op, "!=", op_size) == 0) {
			(*val)->boolean.boolean = !same_types;
			goto done;
		}

		return error(flamingo, "can only check for equality to none value");
	}

	if (kind == FLAMINGO_VAL_KIND_INT) {
		// Integer arithmetic.

		if (strncmp(op, "+", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = left_val->integer.integer + right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "-", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = left_val->integer.integer - right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "*", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = left_val->integer.integer * right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "/", op_size) == 0) {
			if (right_val->integer.integer == 0) {
				return error(flamingo, "division by zero");
			}

			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = left_val->integer.integer / right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "%", op_size) == 0) {
			if (right_val->integer.integer == 0) {
				return error(flamingo, "modulo by zero");
			}

			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = left_val->integer.integer % right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "**", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_INT;
			(*val)->integer.integer = pow(left_val->integer.integer, right_val->integer.integer);
			goto done;
		}

		// Comparisons.

		if (equality(op, op_size, left_val, right_val, val)) {
			goto done;
		}

		if (strncmp(op, "<", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->integer.integer < right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, "<=", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->integer.integer <= right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, ">", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->integer.integer > right_val->integer.integer;
			goto done;
		}

		if (strncmp(op, ">=", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->integer.integer >= right_val->integer.integer;
			goto done;
		}
	}

	if (kind == FLAMINGO_VAL_KIND_BOOL) {
		// Logical operators.

		if (strncmp(op, "&&", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->boolean.boolean && right_val->boolean.boolean;
			goto done;
		}

		if (strncmp(op, "||", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = left_val->boolean.boolean || right_val->boolean.boolean;
			goto done;
		}

		if (strncmp(op, "^^", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_BOOL;
			(*val)->boolean.boolean = (!!left_val->boolean.boolean) ^ (!!right_val->boolean.boolean);
			goto done;
		}

		if (equality(op, op_size, left_val, right_val, val)) {
			goto done;
		}
	}

	if (kind == FLAMINGO_VAL_KIND_STR) {
		// String concatenation.
		// TODO Multiplication (but then I need to rethink the whole operands having to have the same type thing). (For vectors too.)

		if (strncmp(op, "+", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_STR;

			(*val)->str.size = left_val->str.size + right_val->str.size;
			(*val)->str.str = malloc((*val)->str.size);
			assert((*val)->str.str != NULL);

			memcpy((*val)->str.str, left_val->str.str, left_val->str.size);
			memcpy((*val)->str.str + left_val->str.size, right_val->str.str, right_val->str.size);

			goto done;
		}

		if (equality(op, op_size, left_val, right_val, val)) {
			goto done;
		}
	}

	if (kind == FLAMINGO_VAL_KIND_VEC) {
		// Vector concatenation.

		if (strncmp(op, "+", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_VEC;

			(*val)->vec.count = left_val->vec.count + right_val->vec.count;
			(*val)->vec.elems = malloc((*val)->vec.count);
			assert((*val)->vec.elems != NULL);

			// Copy all the elements from the left vector.

			for (size_t i = 0; i < left_val->vec.count; i++) {
				(*val)->vec.elems[i] = val_copy(left_val->vec.elems[i]);
			}

			// Copy all the elements from the right vector.

			for (size_t i = left_val->vec.count; i < (*val)->vec.count; i++) {
				(*val)->vec.elems[i] = val_copy(right_val->vec.elems[i - left_val->vec.count]);
			}

			goto done;
		}

		if (equality(op, op_size, left_val, right_val, val)) {
			goto done;
		}
	}

	if (kind == FLAMINGO_VAL_KIND_MAP) {
		// Map concatenation.
		// TODO There's a small issue here: how to handle different values for the same key when adding? Should there be a preference to keep either the left or the right side? Is that too much of a "quirk" to be something I wanna do? Should this ability just be removed entirely?

		if (strncmp(op, "+", op_size) == 0) {
			(*val)->kind = FLAMINGO_VAL_KIND_MAP;

			(*val)->map.count = left_val->map.count + right_val->map.count;

			(*val)->map.keys = malloc((*val)->map.count);
			assert((*val)->map.keys != NULL);

			(*val)->map.vals = malloc((*val)->map.count);
			assert((*val)->map.vals != NULL);

			// Copy all key-value pairs from the left vector.

			for (size_t i = 0; i < left_val->map.count; i++) {
				(*val)->map.keys[i] = val_copy(left_val->map.keys[i]);
				(*val)->map.vals[i] = val_copy(left_val->map.vals[i]);
			}

			// Copy all key-value pairs from the right vector.

			for (size_t i = left_val->map.count; i < (*val)->map.count; i++) {
				(*val)->map.keys[i] = val_copy(right_val->map.keys[i - left_val->map.count]);
				(*val)->map.vals[i] = val_copy(right_val->map.vals[i - left_val->map.count]);
			}

			goto done;
		}

		if (equality(op, op_size, left_val, right_val, val)) {
			goto done;
		}
	}

	// XXX We don't actually need to decref if there's an error, as the flamingo engine will anyway be entirely freed.
	//     This is robust w.r.t. failures in imported flamingo engines, since we fail if the imported program fails (so the scope is freed instantly).

	return error(flamingo, "unknown operator '%.*s' for type %s", (int) op_size, op, val_type_str(left_val));

done:

	val_decref(left_val);
	val_decref(right_val);

	return 0;
}
