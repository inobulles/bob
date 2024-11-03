// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <common.h>
#include <cookie.h>
#include <logging.h>
#include <task.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <sys/stat.h>

#define CC "Cc"

typedef struct {
	flamingo_val_t* flags;
} state_t;

typedef struct {
	state_t* state;
	pthread_mutex_t out_lock;

	flamingo_val_t* src_vec;
	flamingo_val_t* out_vec;
} build_step_state_t;

typedef struct {
	build_step_state_t* bss;

	char* src;
	char* out;
} compile_task_t;

static bool compile_task(void* data) {
	compile_task_t* const task = data;
	bool stop = false;

	// Create command.

	char* cc = getenv("CC");
	cc = cc == NULL ? "cc" : cc;

	cmd_t cmd;
	cmd_create(&cmd, cc, "-fdiagnostics-color=always", "-c", task->src, "-o", task->out, NULL);

	// Add flags.

	flamingo_val_t* const flags = task->bss->state->flags;

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];
		cmd_addf(&cmd, "%.*s", (int) flag->str.size, flag->str.str);
	}

	// Actually execute it.

	pthread_mutex_lock(&task->bss->out_lock);
	LOG_INFO("%s: compiling...", task->src);
	pthread_mutex_unlock(&task->bss->out_lock);

	if (cmd_exec(&cmd) < 0) {
		stop = true;
	}

	pthread_mutex_lock(&task->bss->out_lock);
	cmd_log(&cmd, task->out, task->src, "compile", "compiled");

	// If we're going to stop all other tasks, keep the logging mutex locked so no other messages are printed.

	if (!stop) {
		pthread_mutex_unlock(&task->bss->out_lock);
	}

	// Cleanup.

	cmd_free(&cmd);

	free(task->src);
	free(task->out);

	free(task);
	return stop;
}

typedef enum {
	VALIDATION_RES_ERR,
	VALIDATION_RES_SKIP,
	VALIDATION_RES_COMPILE,
} validation_res_t;

static validation_res_t validate_requirements(char* src, char* out) {
	// TODO Another thing to consider is that I'm not sure if a moved file also updates its modification timestamp (i.e. src/main.c is updated by 'mv src/{other,main}.c').

	// Get last modification times of source and output files.
	// If output file doesn't exist, we need to compile.

	struct stat src_sb;

	if (stat(src, &src_sb) < 0) {
		LOG_FATAL(CC ".compile: Failed to stat source file '%s': %s", src, strerror(errno));
		return VALIDATION_RES_ERR;
	}

	struct stat out_sb;

	if (stat(out, &out_sb) < 0) {
		if (errno != ENOENT) {
			LOG_FATAL(CC ".compile: Failed to stat output file '%s': %s", out, strerror(errno));
			return VALIDATION_RES_ERR;
		}

		return VALIDATION_RES_COMPILE;
	}

	// If source file is newer than output file, we need to compile.
	// Strict comparison because if b is built right after a, we don't want to rebuild b d'office.
	// XXX There is a case where we could build, modify, and build again in the space of one minute in which case changes won't be reflected, but that's such a small edgecase I don't think it's worth letting the complexity spirit demon enter.

	if (src_sb.st_mtime > out_sb.st_mtime) {
		return VALIDATION_RES_COMPILE;
	}

	// TODO Compile if flags have changed.

	return VALIDATION_RES_SKIP;
}

static int compile_step(size_t data_count, void** data) {
	pool_t pool;
	pool_init(&pool, 11); // TODO 11 should be figured out automatically or come from a '-j' flag.
	int rv = -1;

	for (size_t i = 0; i < data_count; i++) {
		build_step_state_t* const bss = data[i];

		for (size_t j = 0; j < bss->src_vec->vec.count; j++) {
			flamingo_val_t* const src_val = bss->src_vec->vec.elems[j];
			flamingo_val_t* const out_val = bss->out_vec->vec.elems[j];

			char* const src = strndup(src_val->str.str, src_val->str.size);
			char* const out = strndup(out_val->str.str, out_val->str.size);

			assert(src != NULL);
			assert(out != NULL);

			validation_res_t const vres = validate_requirements(src, out);

			if (vres == VALIDATION_RES_COMPILE) {
				compile_task_t* const data = malloc(sizeof *data);
				assert(data != NULL);

				data->bss = bss;

				data->src = src;
				data->out = out;

				pool_add_task(&pool, compile_task, data);
				continue;
			}

			if (vres == VALIDATION_RES_SKIP) {
				LOG_SUCCESS("%s: Already compiled.", src);
			}

			free(src);
			free(out);

			if (vres == VALIDATION_RES_ERR) {
				goto done;
			}
		}
	}

	rv = pool_wait(&pool);

done:

	pool_free(&pool);
	return rv;
}

static int prep_compile(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
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

		char* const cookie = gen_cookie(src->str.str, src->str.size, "o");
		flamingo_val_t* const cookie_val = flamingo_val_make_cstr(cookie);
		free(cookie);

		(*rv)->vec.elems[i] = cookie_val;
	}

	// Add build step.

	build_step_state_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;
	pthread_mutex_init(&bss->out_lock, NULL);

	bss->src_vec = srcs;
	bss->out_vec = *rv;

	return add_build_step((uint64_t) state, "C source file compilation", compile_step, bss);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "compile", callable->name_size) == 0) {
		return prep_compile(state, args, rv);
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
