// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <cookie.h>
#include <frugal.h>
#include <fsutil.h>
#include <install.h>
#include <logging.h>
#include <ncpu.h>
#include <pool.h>
#include <str.h>

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
	pthread_mutex_t logging_lock;

	flamingo_val_t* src_vec;
	flamingo_val_t* out_vec;
} build_step_state_t;

typedef struct {
	build_step_state_t* bss;

	char* src;
	char* out;
} compile_task_t;

static void add_flags(cmd_t* cmd, compile_task_t* task) {
	flamingo_val_t* const flags = task->bss->state->flags;

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];
		cmd_addf(cmd, "%.*s", (int) flag->str.size, flag->str.str);
	}
}

static void get_include_deps(compile_task_t* task, char* cc) {
	// Figure out the include dependencies.
	// For this, parse the Makefile rule output by the preprocessor.
	// See: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html#Preprocessor-Options
	// Also see: https://wiki.sei.cmu.edu/confluence/display/c/STR06-C.+Do+not+assume+that+strtok%28%29+leaves+the+parse+string+unchanged
	// -MM: Output the dependencies to stdout, and imply the -E switch (i.e. preprocess only).
	// -MT "": Exclude the source file from the output.
	// TODO Can we do this in parallel easily with 'cmd_exec_async'?

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, cc, "-fdiagnostics-color=always", "-MM", "-MT", "", task->src, NULL);
	add_flags(&cmd, task);

	int const rv = cmd_exec(&cmd);
	char* STR_CLEANUP out = cmd_read_out(&cmd);

	if (rv < 0) {
		LOG_WARN("Couldn't figure out include dependencies for %s - modifications to included files will not trigger a rebuild!", task->src, cc);
		return;
	}

	// Open file for writing out include deps.

	char deps_path[strlen(task->out) + 6];
	snprintf(deps_path, sizeof deps_path, "%s.deps", task->out);

	FILE* const f = fopen(deps_path, "w");

	if (f == NULL) {
		LOG_WARN("Failed to open '%s' for writing: %s", deps_path, strerror(errno));
		return;
	}

	// Now, actually parse.

	char* headers = out;
	char* header;

	while ((header = strsep(&headers, " ")) != NULL) {
		if (*header == ':' || *header == '\\' || !*header) {
			continue;
		}

		size_t const len = strlen(header);

		if (header[len - 1] == '\n') {
			header[len - 1] = '\0';
		}

		fprintf(f, "%s\n", header);
	}

	fclose(f);
	set_owner(deps_path);
}

static bool compile_task(void* data) {
	compile_task_t* const task = data;
	bool stop = false;

	// Log that we're compiling.

	pthread_mutex_lock(&task->bss->logging_lock);
	LOG_INFO("%s" CLEAR ": Compiling...", task->src);
	pthread_mutex_unlock(&task->bss->logging_lock);

	// Get compiler command to use.

	char* cc = getenv("CC");
	cc = cc == NULL ? "cc" : cc;

	// Get the include dependencies.

	get_include_deps(task, cc);

	// Run compilation command.

	cmd_t cmd;
	cmd_create(&cmd, cc, "-fdiagnostics-color=always", "-c", task->src, "-o", task->out, NULL);
	add_flags(&cmd, task);

	if (cmd_exec(&cmd) < 0) {
		stop = true;
	}

	else {
		set_owner(task->out);
	}

	pthread_mutex_lock(&task->bss->logging_lock);
	cmd_log(&cmd, task->out, task->src, "compile", "compiled", true);
	pthread_mutex_unlock(&task->bss->logging_lock);

	if (!stop && install_cookie(task->out) < 0) {
		stop = true;
	}

	cmd_free(&cmd);

	// Cleanup.

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

static validation_res_t validate_requirements(flamingo_val_t* flags, char* src, char* out) {
	// Compile if flags have changed.
	// Do this first as 'frugal_flags' also writes out the flags file.
	// We can't do this at the level of the 'Cc' instance, because that wouldn't handle the case where we move a source file between different 'Cc's without changing their flags (but from the point of view of the source file the flags have indeed changed).

	if (frugal_flags(flags, out)) {
		return VALIDATION_RES_COMPILE;
	}

	// Create initial dependency list with just the source.
	// Make sure this one is first so it short-circuits 'frugal_mtime'.

	size_t dep_count = 1;
	char** deps = malloc(sizeof *deps);
	assert(deps != NULL);
	deps[0] = src;

	// Look for include dependencies and add them as dependencies.

	char deps_path[strlen(out) + 6];
	snprintf(deps_path, sizeof deps_path, "%s.deps", out);

	FILE* const f = fopen(deps_path, "r");

	if (f == NULL) {
		free(deps);
		return VALIDATION_RES_COMPILE;
	}

	fseek(f, 0, SEEK_END);
	long const deps_size = ftell(f);

	char* STR_CLEANUP headers = malloc(deps_size + 1);
	assert(headers != NULL);

	rewind(f);
	fread(headers, 1, deps_size, f);
	headers[deps_size] = '\0';

	fclose(f);

	char* header;

	while ((header = strsep(&headers, "\n")) != NULL) {
		if (*header == '\0') {
			continue;
		}

		deps = realloc(deps, (dep_count + 1) * sizeof *deps);
		assert(deps != NULL);

		deps[dep_count++] = header;
	}

	// Check modification times between dependencies and target.

	bool do_compile;

	if (frugal_mtime(&do_compile, CC ".compile", dep_count, deps, out) < 0) {
		free(deps);
		return VALIDATION_RES_ERR;
	}

	free(deps);
	return do_compile ? VALIDATION_RES_COMPILE : VALIDATION_RES_SKIP;
}

static int compile_step(size_t data_count, void** data, char const* preinstall_prefix) {
	pool_t pool;
	pool_init(&pool, ncpu());
	int rv = -1;

	for (size_t i = 0; i < data_count; i++) {
		build_step_state_t* const bss = data[i];
		assert(bss->src_vec->vec.count == bss->out_vec->vec.count);

		for (size_t j = 0; j < bss->src_vec->vec.count; j++) {
			flamingo_val_t* const src_val = bss->src_vec->vec.elems[j];
			flamingo_val_t* const out_val = bss->out_vec->vec.elems[j];

			char* const src = strndup(src_val->str.str, src_val->str.size);
			char* const out = strndup(out_val->str.str, out_val->str.size);

			assert(src != NULL);
			assert(out != NULL);

			validation_res_t vres = validate_requirements(bss->state->flags, src, out);

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
				log_already_done(out, src, "compiled");

				if (install_cookie(out) < 0) {
					vres = VALIDATION_RES_ERR;
				}
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
	pthread_mutex_init(&bss->logging_lock, NULL);

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
