// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

static int parse_map(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "map") == 0);

	size_t const n = ts_node_child_count(node);

	size_t count = 0;
	flamingo_val_t** keys = NULL;
	flamingo_val_t** vals = NULL;

	for (size_t i = 0; i < n; i++) {
		TSNode const child = ts_node_child(node, i);
		char const* const child_type = ts_node_type(child);

		if (strcmp(child_type, "map_item") != 0) {
			continue;
		}

		TSNode const key_node = ts_node_child_by_field_name(child, "key", 3);
		TSNode const val_node = ts_node_child_by_field_name(child, "value", 5);

		assert(strcmp(ts_node_type(key_node), "expression") == 0);
		assert(strcmp(ts_node_type(val_node), "expression") == 0);

		// Make room for a new key-value pair.

		count++;

		keys = realloc(keys, count * sizeof *keys);
		assert(keys != NULL);

		vals = realloc(vals, count * sizeof *vals);
		assert(vals != NULL);

		// Parse key expression.

		flamingo_val_t* k = NULL;

		if (parse_expr(flamingo, key_node, &k, NULL) < 0) {
			free(keys);
			free(vals);

			return -1;
		}

		// Make sure key doesn't already exist in map.

		for (size_t j = 0; j < count - 1; j++) {
			if (val_eq(keys[j], k)) {
				free(keys);
				free(vals);

				return error(flamingo, "duplicate key in map");
			}
		}

		// Parse value expression.

		flamingo_val_t* v = NULL;

		if (parse_expr(flamingo, val_node, &v, NULL) < 0) {
			free(keys);
			free(vals);

			return -1;
		}

		keys[count - 1] = k;
		vals[count - 1] = v;
	}

	if (val == NULL) {
		for (size_t i = 0; i < count; i++) {
			val_decref(keys[i]);
			val_decref(vals[i]);
		}

		free(keys);
		free(vals);

		return 0;
	}

	assert(*val == NULL);
	*val = val_alloc();

	(*val)->kind = FLAMINGO_VAL_KIND_MAP;
	(*val)->map.count = count;
	(*val)->map.keys = keys;
	(*val)->map.vals = vals;

	return 0;
}
