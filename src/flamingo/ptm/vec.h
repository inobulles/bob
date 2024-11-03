// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../val.h"
#include "../var.h"

#include "../grammar/call.h"

static inline int vec_len(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_VEC);

	if (args->count != 0) {
		return error(flamingo, "'vec.len' expected 0 arguments, got %zu", args->count);
	}

	*rv = flamingo_val_make_int(self->vec.count);
	return 0;
}

static inline int vec_map(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_VEC);

	// Check our arguments.

	if (args->count != 1) {
		return error(flamingo, "'vec.map' expected 1 argument, got %zu", args->count);
	}

	flamingo_val_t* const fn = args->args[0];

	if (fn->kind != FLAMINGO_VAL_KIND_FN) {
		return error(flamingo, "'vec.map' expected 'fn' argument to be a function, got a %s", val_type_str(fn));
	}

	// Actually map the vector.

	flamingo_val_t* const vec = val_alloc();
	vec->kind = FLAMINGO_VAL_KIND_VEC;
	vec->vec.count = self->vec.count;
	vec->vec.elems = calloc(vec->vec.count, sizeof *vec->vec.elems);
	assert(vec->vec.elems != NULL);

	for (size_t i = 0; i < vec->vec.count; i++) {
		flamingo_val_t* const elem = self->vec.elems[i];
		flamingo_val_t* const args[] = {elem};

		flamingo_arg_list_t arg_list = {
			.count = 1,
			.args = (void*) args,
		};

		if (call(flamingo, fn, NULL, &vec->vec.elems[i], &arg_list) < 0) {
			val_free(vec);
			return -1;
		}
	}

	*rv = vec;

	return 0;
}

static inline int vec_where(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_VEC);

	// Check our arguments.

	if (args->count != 1) {
		return error(flamingo, "'vec.where' expected 1 argument, got %zu", args->count);
	}

	flamingo_val_t* const fn = args->args[0];

	if (fn->kind != FLAMINGO_VAL_KIND_FN) {
		return error(flamingo, "'vec.where' expected 'fn' argument to be a function, got a %s", val_type_str(fn));
	}

	// Actually filter the vector.

	flamingo_val_t* const vec = val_alloc();
	vec->kind = FLAMINGO_VAL_KIND_VEC;
	vec->vec.count = 0;
	vec->vec.elems = NULL;

	for (size_t i = 0; i < self->vec.count; i++) {
		flamingo_val_t* const elem = self->vec.elems[i];
		flamingo_val_t* const args[] = {elem};

		flamingo_arg_list_t arg_list = {
			.count = 1,
			.args = (void*) args,
		};

		flamingo_val_t* keep = NULL;

		if (call(flamingo, fn, NULL, &keep, &arg_list) < 0) {
			val_free(vec);
			return -1;
		}

		if (keep->kind != FLAMINGO_VAL_KIND_BOOL) {
			val_free(keep);
			val_free(vec);
			return error(flamingo, "'vec.where' expected 'fn' to return a boolean, got a %s", val_type_str(keep));
		}

		if (!keep->boolean.boolean) {
			val_free(keep);
			continue;
		}

		val_free(keep);

		vec->vec.elems = realloc(vec->vec.elems, ++vec->vec.count * sizeof *vec->vec.elems);
		assert(vec->vec.elems != NULL);
		vec->vec.elems[vec->vec.count - 1] = val_copy(elem);
	}

	*rv = vec;

	return 0;
}
