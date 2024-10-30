// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <class/class.h>
#include <logging.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>

#define CC "CC"

typedef struct {
	flamingo_val_t* flags;
} state_t;

static int compile(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	if (args->count != 1) {
		LOG_FATAL(CC ".compile: Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL(CC ".compile: Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const srcs = args->args[0];

	for (size_t i = 0; i < srcs->vec.count; i++) {
		flamingo_val_t* const src = srcs->vec.elems[i];

		if (src->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL(CC ".compile: Expected %zu-th vector element to be a string", i);
			return -1;
		}

		printf("TODO Compile source: %.*s\n", (int) src->str.size, src->str.str);
	}

	return 0;
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (flamingo_cstrcmp(callable->name, "compile", callable->name_size) == 0) {
		return compile(args, rv);
	}

	*consumed = false;
	return 0;
}

static void free_state(flamingo_val_t* inst, void* data) {
	state_t* const state = data;
	free(state);
}

static int instantiate(flamingo_val_t* inst, flamingo_arg_list_t* args) {
	// Validate flags argument.

	if (args->count != 1) {
		LOG_FATAL(CC ": Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL(CC ": Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const flags = args->args[0];

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];

		if (flag->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL(CC ": Expected %zu-th vector element to be a string", i);
			return -1;
		}
	}

	// Create state object.

	state_t* const state = malloc(sizeof *state);
	assert(state != NULL);

	state->flags = flags;

	inst->inst.data = state;
	inst->inst.free_data = free_state;

	return 0;
}

bob_class_t BOB_CLASS_CC = {
	.name = CC,
	.populate = NULL,
	.call = call,
	.instantiate = instantiate,
};
