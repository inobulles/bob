// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <apple.h>
#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <cookie.h>
#include <frugal.h>
#include <fsutil.h>
#include <install.h>
#include <logging.h>
#include <pool.h>
#include <str.h>

#include <assert.h>
#include <fts.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>

#define LINKER "Linker"

typedef struct {
	flamingo_val_t* flags;
} state_t;

typedef struct {
	state_t* state;
	bool archive;

	char const* log_prefix;
	char const* infinitive;
	char const* present;
	char const* past;

	flamingo_val_t* src_vec;
	flamingo_val_t* out_str;
} build_step_state_t;

static int link_step(size_t data_count, void** data) {
	assert(data_count == 1); // See comment just before 'add_build_step' in 'prep_link'.

	build_step_state_t* const bss = *data;
	int rv = -1;

	// Make everything C strings.

	char* const STR_CLEANUP out = strndup(bss->out_str->str.str, bss->out_str->str.size);
	assert(out != NULL);

	size_t const src_count = bss->src_vec->vec.count;
	char* srcs[src_count];
	memset(srcs, 0, sizeof srcs);

	for (size_t i = 0; i < src_count; i++) {
		flamingo_val_t* const src_val = bss->src_vec->vec.elems[i];
		srcs[i] = strndup(src_val->str.str, src_val->str.size);
		assert(srcs[i] != NULL);
	}

	char* const STR_CLEANUP pretty = cookie_to_output(out, NULL);

	// Re-link if flags have changed.

	if (frugal_flags(bss->state->flags, out)) {
		goto link;
	}

	// Re-link if any statically linked dependencies have changed.
	// We know we have a static dependency when there's a cookie in the flags.

	for (size_t i = 0; i < bss->state->flags->vec.count; i++) {
		flamingo_val_t* const flag = bss->state->flags->vec.elems[i];

		if (has_built_cookie(flag->str.str, flag->str.size)) {
			goto link;
		}
	}

	// Check modification times.

	bool do_link;

	if (frugal_mtime(&do_link, bss->log_prefix, src_count, srcs, out) < 0) {
		goto link;
	}

	if (do_link) {
		goto link;
	}

	// Already linked.

	log_already_done(out, pretty, bss->past);
	rv = install_cookie(out);

	goto done;

link:;

	// Create command.

	cmd_t cmd;

	if (bss->archive) {
		char* ar = getenv("AR");
		ar = ar == NULL ? "ar" : ar;
		cmd_create(&cmd, ar, "-rcs", out, NULL);
	}

	else {
		char* cc = getenv("CC");
		cc = cc == NULL ? "cc" : cc;
		cmd_create(&cmd, cc, "-fdiagnostics-color=always", "-o", out, NULL);
		cmd_addf(&cmd, "-L%s/lib", install_prefix);

#if defined(__APPLE__)
		cmd_add(&cmd, "-rpath");
		cmd_add(&cmd, "@loader_path/..");
#endif
	}

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

	if (pretty == NULL) {
		LOG_INFO(CLEAR "%s...", bss->present);
	}
	else {
		LOG_INFO("%s" CLEAR ": %s...", pretty, bss->present);
	}

	rv = cmd_exec(&cmd);

	if (rv == 0) {
		set_owner(out);
	}

	cmd_log(&cmd, out, pretty, bss->infinitive, bss->past, true);
	cmd_free(&cmd);

	if (rv == 0) {
		rv = install_cookie(out);
	}

done:

	for (size_t i = 0; i < bss->src_vec->vec.count; i++) {
		free(srcs[i]);
	}

	return rv;
}

static int prep_link(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv, bool archive) {
	char const* const log_prefix = archive ? LINKER ".archive" : LINKER ".link";
	char const* const infinitive = archive ? "archive" : "link";
	char const* const present = archive ? "Archiving" : "Linking";
	char const* const past = archive ? "archived" : "linked";

	// Validate sources argument.

	if (args->count != 1) {
		LOG_FATAL("%s: Expected 1 argument, got %zu", log_prefix, args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL("%s: Expected argument to be a vector", log_prefix);
		return -1;
	}

	flamingo_val_t* const srcs = args->args[0];

	// Return single output cookie (hash of all the inputs).

	uint64_t total_hash = 0;

	for (size_t i = 0; i < srcs->vec.count; i++) {
		flamingo_val_t* const src = srcs->vec.elems[i];

		if (src->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("%s: Expected %zu-th vector element to be a string", log_prefix, i);
			return -1;
		}

		// XOR'ing makes sure order doesn't matter.

		total_hash ^= str_hash(src->str.str, src->str.size);
	}

	char* cookie = NULL;
	asprintf(&cookie, "%s/bob/linker.%s.cookie.%" PRIx64 ".%s", out_path, infinitive, total_hash, archive ? "a" : "l");
	assert(cookie != NULL);
	*rv = flamingo_val_make_cstr(cookie);

	// Add build step.

	build_step_state_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;
	bss->archive = archive;

	bss->log_prefix = log_prefix;
	bss->infinitive = infinitive;
	bss->present = present;
	bss->past = past;

	bss->src_vec = srcs;
	bss->out_str = *rv;

	// We never want to merge these build steps because the output hash from one set of source files to another is (hopefully) always different.

	return add_build_step((uint64_t) bss, present, link_step, bss);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "link", callable->name_size) == 0) {
		return prep_link(state, args, rv, false);
	}

	else if (flamingo_cstrcmp(callable->name, "archive", callable->name_size) == 0) {
		return prep_link(state, args, rv, true);
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
