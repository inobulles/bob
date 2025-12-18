// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

static int parse_vec(flamingo_t* flamingo, TSNode node, flamingo_val_t** val) {
	assert(strcmp(ts_node_type(node), "vec") == 0);

	size_t const n = ts_node_child_count(node);

	size_t elem_count = 0;
	flamingo_val_t** elems = NULL;

	for (size_t i = 0; i < n; i++) {
		TSNode const child = ts_node_child(node, i);
		char const* const child_type = ts_node_type(child);

		if (strcmp(child_type, "expression") != 0) {
			continue;
		}

		elems = realloc(elems, ++elem_count * sizeof *elems);
		assert(elems != NULL);

		flamingo_val_t* elem = NULL;

		if (parse_expr(flamingo, child, &elem, NULL) < 0) {
			free(elems);
			return -1;
		}

		elems[elem_count - 1] = elem;
	}

	if (val == NULL) {
		for (size_t i = 0; i < elem_count; i++) {
			val_decref(elems[i]);
		}

		free(elems);
		return 0;
	}

	assert(*val == NULL);
	*val = val_alloc();

	(*val)->kind = FLAMINGO_VAL_KIND_VEC;
	(*val)->vec.count = elem_count;
	(*val)->vec.elems = elems;

	return 0;
}
