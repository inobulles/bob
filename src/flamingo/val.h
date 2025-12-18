// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo
// Copyright (c) 2025 Drake Fletcher

/*
 * Values.
 *
 * Values are the fundamental data units in Flamingo.
 * They are reference-counted and can represent various types, including primitives (integers, strings, booleans), collections (vectors, maps), and callables (functions, classes).
 *
 * Each value can have an optional name and an "owner" scope, which is used for memory management and debugging.
 */

#pragma once

#include "common.h"
#include "env.h"
#include "scope.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static flamingo_val_t* val_incref(flamingo_val_t* val) {
	assert(val->ref_count > 0); // value has already been freed
	val->ref_count++;

	return val;
}

static flamingo_val_t* val_init(flamingo_val_t* val) {
	// By default, values are anonymous.

	val->name = NULL;
	val->name_size = 0;

	// By default, values are nones.

	val->kind = FLAMINGO_VAL_KIND_NONE;
	val->ref_count = 1;

	val->owner = NULL;

	return val;
}

static char const* val_type_str(flamingo_val_t const* val) {
	switch (val->kind) {
	case FLAMINGO_VAL_KIND_BOOL:
		return "boolean";
	case FLAMINGO_VAL_KIND_INT:
		return "integer";
	case FLAMINGO_VAL_KIND_STR:
		return "string";
	case FLAMINGO_VAL_KIND_VEC:
		return "vector";
	case FLAMINGO_VAL_KIND_MAP:
		return "map";
	case FLAMINGO_VAL_KIND_FN:
		switch (val->fn.kind) {
		case FLAMINGO_FN_KIND_EXTERN:
			return "external function";
		case FLAMINGO_FN_KIND_FUNCTION:
			return "function";
		case FLAMINGO_FN_KIND_CLASS:
			return "class";
		case FLAMINGO_FN_KIND_PTM:
			return "primitive type member";
		default:
			assert(false);
		}
	case FLAMINGO_VAL_KIND_NONE:
		return "none";
	case FLAMINGO_VAL_KIND_INST:
		return "instance";
	default:
		return "unknown";
	}
}

static char const* val_role_str(flamingo_val_t* val) {
	switch (val->kind) {
	case FLAMINGO_VAL_KIND_BOOL:
	case FLAMINGO_VAL_KIND_INT:
	case FLAMINGO_VAL_KIND_STR:
	case FLAMINGO_VAL_KIND_NONE:
	case FLAMINGO_VAL_KIND_INST:
		return "variable";
	case FLAMINGO_VAL_KIND_FN:
		return val_type_str(val);
	default:
		return "unknown";
	}
}

static flamingo_val_t* val_alloc(void) {
	flamingo_val_t* const val = calloc(1, sizeof *val);
	assert(val != NULL);

	return val_init(val);
}

static flamingo_val_t* val_copy(flamingo_val_t* val) {
	flamingo_val_t* const copy = calloc(1, sizeof *copy);
	assert(copy != NULL);

	memcpy(copy, val, sizeof *val);
	copy->ref_count = 1;

	if (val->name != NULL) {
		copy->name = strndup(val->name, val->name_size);
		assert(copy->name != NULL);
	}

	switch (copy->kind) {
	case FLAMINGO_VAL_KIND_NONE:
	case FLAMINGO_VAL_KIND_BOOL:
	case FLAMINGO_VAL_KIND_INT:
		break;
	case FLAMINGO_VAL_KIND_STR:
		copy->str.str = strndup(val->str.str, val->str.size);
		assert(copy->str.str != NULL);
		break;
	case FLAMINGO_VAL_KIND_VEC:
		copy->vec.elems = malloc(val->vec.count * sizeof *copy->vec.elems);
		assert(copy->vec.elems != NULL);

		for (size_t i = 0; i < val->vec.count; i++) {
			copy->vec.elems[i] = val_incref(val->vec.elems[i]);
		}

		break;
	case FLAMINGO_VAL_KIND_MAP:
		copy->map.keys = malloc(val->map.count * sizeof *copy->map.keys);
		assert(copy->map.keys != NULL);

		copy->map.vals = malloc(val->map.count * sizeof *copy->map.vals);
		assert(copy->map.vals != NULL);

		for (size_t i = 0; i < val->map.count; i++) {
			copy->map.keys[i] = val_incref(val->map.keys[i]);
			copy->map.vals[i] = val_incref(val->map.vals[i]);
		}

		break;
	case FLAMINGO_VAL_KIND_FN:
		if (val->fn.env != NULL) {
			copy->fn.env = env_close_over(val->fn.env);
		}

		break;
	case FLAMINGO_VAL_KIND_INST:
		if (val->inst.scope != NULL) {
			val->inst.scope->ref_count++;
		}

		break;
	case FLAMINGO_VAL_KIND_COUNT:
		break;
	}

	return copy;
}

static bool val_eq(flamingo_val_t* x, flamingo_val_t* y) {
	if (x->kind != y->kind) {
		return false;
	}

	// Don't check that the names are equal here!

	flamingo_val_kind_t const kind = x->kind;

	switch (kind) {
	case FLAMINGO_VAL_KIND_NONE:
		return true;
	case FLAMINGO_VAL_KIND_BOOL:
		return x->boolean.boolean == y->boolean.boolean;
	case FLAMINGO_VAL_KIND_INT:
		return x->integer.integer == y->integer.integer;
	case FLAMINGO_VAL_KIND_STR:
		return flamingo_strcmp(x->str.str, y->str.str, x->str.size, y->str.size) == 0;
	case FLAMINGO_VAL_KIND_VEC:
		if (x->vec.count != y->vec.count) {
			return false;
		}

		for (size_t i = 0; i < x->vec.count; i++) {
			flamingo_val_t* const x_val = x->vec.elems[i];
			flamingo_val_t* const y_val = y->vec.elems[i];

			if (!val_eq(x_val, y_val)) {
				return false;
			}
		}

		return true;
	case FLAMINGO_VAL_KIND_MAP:
		if (x->map.count != y->map.count) {
			return false;
		}

		for (size_t i = 0; i < x->map.count; i++) {
			flamingo_val_t* const k = x->map.keys[i];
			flamingo_val_t* const v = x->map.vals[i];

			// Find key in y.

			bool found = false;

			for (size_t j = 0; j < y->map.count; j++) {
				if (!val_eq(k, y->map.keys[j])) {
					continue;
				}

				if (!val_eq(v, y->map.vals[j])) {
					return false;
				}

				found = true;
				break;
			}

			if (!found) {
				return false;
			}
		}

		return true;
	case FLAMINGO_VAL_KIND_FN:
		return memcmp(&x->fn, &y->fn, sizeof x->fn) == 0;
	case FLAMINGO_VAL_KIND_INST:
		return memcmp(&x->inst, &y->inst, sizeof x->inst) == 0;
	case FLAMINGO_VAL_KIND_COUNT:
		return false;
	}

	return false; // XXX To make GCC happy.
}

static void val_free(flamingo_val_t* val) {
	free(val->name);

	switch (val->kind) {
	case FLAMINGO_VAL_KIND_STR:
		free(val->str.str);
		break;
	case FLAMINGO_VAL_KIND_VEC:
		for (size_t i = 0; i < val->vec.count; i++) {
			val_decref(val->vec.elems[i]);
		}

		free(val->vec.elems);
		break;
	case FLAMINGO_VAL_KIND_MAP:
		for (size_t i = 0; i < val->map.count; i++) {
			val_decref(val->map.keys[i]);
			val_decref(val->map.vals[i]);
		}

		free(val->map.keys);
		free(val->map.vals);

		break;
	case FLAMINGO_VAL_KIND_FN:
		free(val->fn.body);
		free(val->fn.params);

		if (val->fn.env != NULL) {
			env_free(val->fn.env);
		}

		break;
	case FLAMINGO_VAL_KIND_INST:
		scope_decref(val->inst.scope);

		if (val->inst.free_data != NULL) {
			val->inst.free_data(val, val->inst.data);
		}

		break;
	case FLAMINGO_VAL_KIND_BOOL:
	case FLAMINGO_VAL_KIND_INT:
	case FLAMINGO_VAL_KIND_NONE:
		break;
	default:
		assert(false);
	}

	free(val);
}

static flamingo_val_t* val_decref(flamingo_val_t* val) {
	if (val == NULL) {
		return NULL;
	}

	val->ref_count--;

	if (val->ref_count > 0) {
		return val;
	}

	// if value is not referred to by anyone, free it

	val_free(val);
	return NULL;
}
