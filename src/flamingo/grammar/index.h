// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo
// Copyright (c) 2025 Drake Fletcher

#pragma once

#include "expr.h"

#include "../common.h"

static int parse_index(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t*** slot, bool lhs) {
	assert(strcmp(ts_node_type(node), "index") == 0);

	int rv = 0;
	flamingo_val_t* indexed_val = NULL;
	flamingo_val_t* index_val = NULL;

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

	if (parse_expr(flamingo, indexed_node, &indexed_val, NULL) < 0) {
		rv = -1;
		goto cleanup;
	}

	bool const is_vec = indexed_val->kind == FLAMINGO_VAL_KIND_VEC;
	bool const is_map = indexed_val->kind == FLAMINGO_VAL_KIND_MAP;

	if (!is_vec && !is_map) {
		rv = error(flamingo, "can only index vectors and maps, got %s", val_type_str(indexed_val));
		goto cleanup;
	}

	// Evaluate index expression.
	// Make sure it can be used as an index if indexing a vector.

	if (parse_expr(flamingo, index_node, &index_val, NULL) < 0) {
		rv = -1;
		goto cleanup;
	}

	if (is_vec && index_val->kind != FLAMINGO_VAL_KIND_INT) {
		rv = error(flamingo, "can only use integers as indices, got %s", val_type_str(index_val));
		goto cleanup;
	}

	// Prepare result value and exit here if we discard it.

	if (val == NULL && slot == NULL) {
		goto cleanup;
	}

	if (val != NULL) {
		assert(*val == NULL);
	}

	if (slot != NULL) {
		*slot = NULL;
	}

	// Do the indexing operation per indexed type.

	if (is_vec) {
		size_t const indexed_count = indexed_val->vec.count;
		flamingo_val_t** const indexed_elems = indexed_val->vec.elems;
		int64_t const index = index_val->integer.integer;

		// Check bounds.

		if (
			(index >= 0 && (size_t) index >= indexed_count) ||
			(index < 0 && (size_t) -index > indexed_count)
		) {
			rv = error(flamingo, "index %" PRId64 " is out of bounds for vector of size %zu", index, indexed_count);
			goto cleanup;
		}

		// Actually index vector.

		flamingo_val_t** const target_slot = &indexed_elems[index >= 0 ? (size_t) index : (size_t) (indexed_count + index)];

		if (val != NULL) {
			*val = *target_slot;
			val_incref(*val);
		}

		if (slot != NULL) {
			*slot = target_slot;
		}

		goto cleanup;
	}

	else if (is_map) {
		size_t indexed_count = indexed_val->map.count;
		flamingo_val_t** const indexed_keys = indexed_val->map.keys;
		flamingo_val_t** const indexed_vals = indexed_val->map.vals;

		// Look for key.

		for (size_t i = 0; i < indexed_count; i++) {
			flamingo_val_t* const key = indexed_keys[i];

			if (val_eq(key, index_val)) {
				if (val != NULL) {
					*val = indexed_vals[i];
					val_incref(*val);
				}

				if (slot != NULL) {
					*slot = &indexed_vals[i];
				}

				goto cleanup;
			}
		}

		// No key found.
		// Create a new value.

		if (val != NULL) {
			*val = val_alloc();
		}

		// Add that new entry to the map if we're on the LHS.

		if (lhs) {
			indexed_count = ++indexed_val->map.count;

			indexed_val->map.keys = realloc(indexed_val->map.keys, indexed_count * sizeof *indexed_val->map.keys);
			assert(indexed_val->map.keys != NULL);

			indexed_val->map.vals = realloc(indexed_val->map.vals, indexed_count * sizeof *indexed_val->map.vals);
			assert(indexed_val->map.vals != NULL);

			indexed_val->map.keys[indexed_count - 1] = index_val;

			// If val was created (new NONE), use it. Otherwise we need a new NONE.
			// But wait, if val was created above, we can use it.
			// If val was NOT requested (val==NULL), we still need to put something in the map.

			flamingo_val_t* const new_val = val != NULL ? *val : val_alloc();
			indexed_val->map.vals[indexed_count - 1] = new_val;

			val_incref(index_val);
			val_incref(new_val);

			if (slot != NULL) {
				*slot = &indexed_val->map.vals[indexed_count - 1];
			}
		}

		goto cleanup;
	}

	assert(false);

cleanup:

	val_decref(indexed_val);
	val_decref(index_val);

	return rv;
}
