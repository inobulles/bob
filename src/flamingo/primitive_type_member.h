// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "common.h"
#include "val.h"
#include "var.h"

#include "ptm/str.h"
#include "ptm/vec.h"

static void primitive_type_member_init(flamingo_t* flamingo) {
	for (size_t i = 0; i < FLAMINGO_VAL_KIND_COUNT; i++) {
		flamingo->primitive_type_members[i].count = 0;
		flamingo->primitive_type_members[i].vars = NULL;
	}
}

static void primitive_type_member_free(flamingo_t* flamingo) {
	for (size_t type = 0; type < FLAMINGO_VAL_KIND_COUNT; type++) {
		size_t const count = flamingo->primitive_type_members[type].count;
		flamingo_var_t* const vars = flamingo->primitive_type_members[type].vars;

		for (size_t i = 0; i < count; i++) {
			flamingo_var_t* const var = &vars[i];
			free(var->key);
		}

		if (vars != NULL) {
			free(vars);
		}
	}
}

static int primitive_type_member_add(flamingo_t* flamingo, flamingo_val_kind_t type, size_t key_size, char* key, flamingo_ptm_cb_t cb) {
	// Make sure primitive type member doesn't already exist for this type.

	size_t count = flamingo->primitive_type_members[type].count;
	flamingo_var_t* vars = flamingo->primitive_type_members[type].vars;

	for (size_t i = 0; i < count; i++) {
		flamingo_var_t* const var = &vars[i];

		if (flamingo_strcmp(var->key, key, var->key_size, key_size) == 0) {
			// XXX A little hacky and I needn't forget to update this if 'val_type_str' gets more stuff but eh.

			flamingo_val_t const dummy = {
				.kind = type,
				.fn = {
						 .kind = FLAMINGO_FN_KIND_FUNCTION,
						 },
			};

			return error(flamingo, "primitive type member '%.*s' already exists on type %s", (int) key_size, key, val_type_str(&dummy));
		}
	}

	// If not, add an entry.

	vars = realloc(vars, ++count * sizeof *vars);
	assert(vars != NULL);
	flamingo_var_t* const var = &vars[count - 1];

	var->key_size = key_size;
	var->key = malloc(key_size);
	assert(var->key != NULL);
	memcpy(var->key, key, key_size);

	flamingo->primitive_type_members[type].count = count;
	flamingo->primitive_type_members[type].vars = vars;

	// Finally, create the actual value.

	flamingo_val_t* const val = val_alloc();
	var_set_val(var, val);

	val->kind = FLAMINGO_VAL_KIND_FN;
	val->fn.kind = FLAMINGO_FN_KIND_PTM;
	val->fn.ptm_cb = cb;
	val->fn.env = NULL;
	val->fn.src = NULL;

	return 0;
}

static int primitive_type_member_std(flamingo_t* flamingo) {
#define ADD(type, key, cb)                                                               \
	do {                                                                                  \
		if (primitive_type_member_add(flamingo, (type), strlen((key)), (key), (cb)) < 0) { \
			return -1;                                                                      \
		}                                                                                  \
	} while (0)

	ADD(FLAMINGO_VAL_KIND_STR, "len", str_len);
	ADD(FLAMINGO_VAL_KIND_STR, "endswith", str_endswith);
	ADD(FLAMINGO_VAL_KIND_STR, "startswith", str_startswith);

	ADD(FLAMINGO_VAL_KIND_VEC, "len", vec_len);
	ADD(FLAMINGO_VAL_KIND_VEC, "map", vec_map);
	ADD(FLAMINGO_VAL_KIND_VEC, "where", vec_where);

	return 0;
}
