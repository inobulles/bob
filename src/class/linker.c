// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <common.h>
#include <cookie.h>
#include <logging.h>
#include <str.h>
#include <pool.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <sys/stat.h>

#define LINKER "Linker"

typedef struct {
	flamingo_val_t* flags;
} state_t;

typedef struct {
	state_t* state;

	flamingo_val_t* src_vec;
	flamingo_val_t* out_str;
} build_step_state_t;

static int link_step(size_t data_count, void** data) {
	assert(data_count == 1); // See comment just before 'add_build_step' in 'prep_link'.

	build_step_state_t* const bss = *data;
	int rv = -1;

	// Make everything C strings.

	char* const CLEANUP_STR out = strndup(bss->out_str->str.str, bss->out_str->str.size);
	assert(out != NULL);

	char* srcs[bss->src_vec->vec.count];
	memset(srcs, 0, sizeof srcs);

	for (size_t i = 0; i < bss->src_vec->vec.count; i++) {
		flamingo_val_t* const src_val = bss->src_vec->vec.elems[i];
		srcs[i] = strndup(src_val->str.str, src_val->str.size);
		assert(srcs[i] != NULL);
	}

	// Get last modification time of output.

	struct stat out_sb;

	if (stat(out, &out_sb) < 0) {
		if (errno == ENOENT) {
			goto link;
		}

		LOG_FATAL(LINKER ".link: Failed to stat output file '%s': %s", out, strerror(errno));
		goto done;
	}

	// Get last modification times of sources.

	for (size_t i = 0; i < bss->src_vec->vec.count; i++) {
		char* const src = srcs[i];
		struct stat src_sb;

		if (stat(src, &src_sb) < 0) {
			LOG_FATAL(LINKER ".link: Failed to stat source file '%s': %s", src, strerror(errno));
			goto done;
		}

		if (src_sb.st_mtime > out_sb.st_mtime) {
			goto link;
		}
	}

	// Already linked.

	log_already_done(out, NULL, "linked");
	rv = 0;
	goto done;

link:;

	// Create command.

	char* cc = getenv("CC");
	cc = cc == NULL ? "cc" : cc;

	cmd_t cmd;
	cmd_create(&cmd, cc, "-fdiagnostics-color=always", "-o", NULL);
	cmd_add(&cmd, out);

	for (size_t i = 0; i < bss->src_vec->vec.count; i++) {
		char* const src = srcs[i];
		cmd_add(&cmd, src);
	}

	// Add flags.

	flamingo_val_t* const flags = bss->state->flags;

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];
		cmd_addf(&cmd, "%.*s", (int) flag->str.size, flag->str.str);
	}

	// Actually execute it.

	LOG_INFO("Linking...");
	rv = cmd_exec(&cmd);
	cmd_log(&cmd, out, NULL, "link", "linked");
	cmd_free(&cmd);

done:

	for (size_t i = 0; i < bss->src_vec->vec.count; i++) {
		free(srcs[i]);
	}

	return rv;
}

static int prep_link(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	// Validate sources argument.

	if (args->count != 1) {
		LOG_FATAL(LINKER ".link: Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL(LINKER ".link: Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const srcs = args->args[0];

	// Return single output cookie (hash of all the inputs).

	uint64_t total_hash = 0;

	for (size_t i = 0; i < srcs->vec.count; i++) {
		flamingo_val_t* const src = srcs->vec.elems[i];

		if (src->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL(LINKER ".link: Expected %zu-th vector element to be a string", i);
			return -1;
		}

		// XOR'ing makes sure order doesn't matter.

		char* const CLEANUP_STR tmp = strndup(src->str.str, src->str.size);
		assert(tmp != NULL);
		total_hash ^= str_hash(src->str.str);
	}

	char* cookie = NULL;
	asprintf(&cookie, "%s/bob/linker.link.cookie.%llx.exe", out_path, total_hash);
	assert(cookie != NULL);
	*rv = flamingo_val_make_cstr(cookie);

	// Add build step.

	build_step_state_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;

	bss->src_vec = srcs;
	bss->out_str = *rv;

	// We never want to merge these build steps because the output hash from one set of source files to another is (hopefully) always different.

	return add_build_step((uint64_t) bss, "Linking", link_step, bss);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "link", callable->name_size) == 0) {
		return prep_link(state, args, rv);
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
		LOG_FATAL(LINKER ": Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL(LINKER ": Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const flags = args->args[0];

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];

		if (flag->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL(LINKER ": Expected %zu-th vector element to be a string", i);
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

bob_class_t BOB_CLASS_LINKER = {
	.name = LINKER,
	.populate = NULL,
	.call = call,
	.instantiate = instantiate,
};
