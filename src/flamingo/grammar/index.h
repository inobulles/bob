// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include <common.h>
#include <scope.h>

static int parse_index(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "index") == 0);

	// Get indexed expression.

	TSNode const indexed_node = ts_node_child_by_field_name(node, "indexed", 7);
	char const* const indexed_type = ts_node_type(indexed_node);

	if (strcmp(indexed_type, "expression") != 0) {
		return error(flamingo, "expected expression for indexed, got %s", indexed_type);
	}

	// Get index expression.

	TSNode const index_node = ts_node_child_by_field_name(node, "index", 5);
	char const* const index_type = ts_node_type(index_node);

	if (strcmp(index_type, "expression") != 0) {
		return error(flamingo, "expected expression for index, got %s", index_type);
	}

	// Evaluate indexed expression.
	// Make sure it is indexable in the first place.

	flamingo_val_t* indexed_val = NULL;

	if (parse_expr(flamingo, indexed_node, &indexed_val, NULL) < 0) {
		return -1;
	}

	bool const is_vec = indexed_val->kind == FLAMINGO_VAL_KIND_VEC;
	bool const is_map = indexed_val->kind == FLAMINGO_VAL_KIND_MAP;

	if (!is_vec && !is_map) {
		return error(flamingo, "can only index vectors and maps, got %s", val_type_str(indexed_val));
	}

	// Evaluate index expression.
	// Make sure it can be used as an index if indexing a vector.

	flamingo_val_t* index_val = NULL;

	if (parse_expr(flamingo, index_node, &index_val, NULL) < 0) {
		return -1;
	}

	if (is_vec && index_val->kind != FLAMINGO_VAL_KIND_INT) {
		return error(flamingo, "can only use integers as indices, got %s", val_type_str(index_val));
	}

	// Prepare result value and exit here if we discard it.

	if (val == NULL) {
		return 0;
	}

	assert(*val == NULL);

	// Do the indexing operation per indexed type.

	if (is_vec) {
		size_t const indexed_count = indexed_val->vec.count;
		flamingo_val_t** const indexed_elems = indexed_val->vec.elems;
		int64_t const index = index_val->integer.integer;

		// Check bounds.

		if (index >= 0 && (size_t) index >= indexed_count) {
			return error(flamingo, "index %" PRId64 " is out of bounds for vector of size %zu", index, indexed_count);
		}

		if (index < 0 && (size_t) -index > indexed_count) {
			return error(flamingo, "index %" PRId64 " is out of bounds for vector of size %zu", index, indexed_count);
		}

		// Actually index vector.

		if (index >= 0) {
			*val = indexed_elems[index];
			val_incref(*val);
		}

		else {
			*val = indexed_elems[indexed_count + index];
			val_incref(*val);
		}

		return 0;
	}

	else if (is_map) {
		size_t const indexed_count = indexed_val->map.count;
		flamingo_val_t** const indexed_keys = indexed_val->map.keys;
		flamingo_val_t** const indexed_vals = indexed_val->map.vals;

		// Look for key.

		for (size_t i = 0; i < indexed_count; i++) {
			flamingo_val_t* const key = indexed_keys[i];

			if (val_eq(key, index_val)) {
				*val = indexed_vals[i];
				val_incref(*val);

				return 0;
			}
		}

		*val = val_alloc();
		(*val)->kind = FLAMINGO_VAL_KIND_NONE;

		return 0;
	}

	assert(false);
	return 0;
}
