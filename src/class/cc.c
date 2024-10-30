// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <build_step.h>
#include <class/class.h>
#include <cookie.h>
#include <logging.h>
#include <task.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>

#define CC "CC"

typedef struct {
	flamingo_val_t* flags;
} state_t;

typedef struct {
	flamingo_val_t* src_vec;
	flamingo_val_t* out_vec;
} build_step_state_t;

typedef struct {
	flamingo_val_t* src;
	flamingo_val_t* out;
} compile_task_t;

static bool compile_task(void* data) {
	compile_task_t* const task = data;

	// TODO.

	printf("compile %.*s -> %.*s\n", (int) task->src->str.size, task->src->str.str, (int) task->out->str.size, task->out->str.str);

	free(task);
	return false;
}

static int compile_step(size_t data_count, void** data) {
	// Strategy:
	// - Figure out if the source file is newer than the output file or the options have changed.
	// - I do realize that if we wanna compare output files correctly, we're going to have to be a little smarter about how we generate the cookies. Probably going to have to do it by path.
	// - Another thing to consider is that I'm not sure if a moved file also updates its modification timestamp (i.e. src/main.c is updated by 'mv src/{other,main}.c').
	// - If so, add compilation task to the task queue.
	// - Once everything has been added to the async task queue, we must wait for all tasks to finish before moving onto the next step.

	// Task queue strategy:
	// - Immediately start compilation process, piping stdout and stderr to the same place.
	// - If there's an error, cancel all the other tasks in the task queue somehow and spit out combined stdout and stderr log.
	// - If there's no error, spit out combined stdout and stderr log (for warnings and the like).
	// - Also if there's no error, write out the flags used.

	pool_t pool;
	pool_init(&pool, 11);

	for (size_t i = 0; i < data_count; i++) {
		build_step_state_t* const bss = data[i];

		for (size_t j = 0; j < bss->src_vec->vec.count; j++) {
			compile_task_t* const data = malloc(sizeof *data);
			assert(data != NULL);

			data->src = bss->src_vec->vec.elems[j];
			data->out = bss->out_vec->vec.elems[j];

			pool_add_task(&pool, compile_task, data);
		}
	}

	int const rv = pool_wait(&pool);

	pool_free(&pool);

	return rv;
}

static int compile(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	// Validate sources argument.

	if (args->count != 1) {
		LOG_FATAL(CC ".compile: Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL(CC ".compile: Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const srcs = args->args[0];

	// Return list of output cookies.

	*rv = flamingo_val_make_none();
	(*rv)->kind = FLAMINGO_VAL_KIND_VEC;

	(*rv)->vec.count = srcs->vec.count;
	(*rv)->vec.elems = malloc((*rv)->vec.count * sizeof *(*rv)->vec.elems);
	assert((*rv)->vec.elems != NULL);

	// Generate each output cookie and validate members of source vector.

	for (size_t i = 0; i < srcs->vec.count; i++) {
		flamingo_val_t* const src = srcs->vec.elems[i];

		if (src->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL(CC ".compile: Expected %zu-th vector element to be a string", i);
			return -1;
		}

		char* const cookie = gen_cookie(src->str.str, src->str.size);
		flamingo_val_t* const cookie_val = flamingo_val_make_cstr(cookie);
		free(cookie);

		(*rv)->vec.elems[i] = cookie_val;
	}

	// Add build step.

	build_step_state_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->src_vec = srcs;
	bss->out_vec = *rv;

	return add_build_step((uint64_t) state, "C source file compilation", compile_step, bss);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "compile", callable->name_size) == 0) {
		return compile(state, args, rv);
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
