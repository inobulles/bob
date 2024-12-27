// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <cookie.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <inttypes.h>

#include "aquarium_common.h"

#define AQUARIUM "Aquarium"
#define MAGIC (strhash(AQUARIUM "() magic"))

typedef struct {
	aquarium_state_t* state;
	char* cmd;
} exec_bss_t;

typedef struct {
	aquarium_state_t* state;
	char* cookie;
} image_bss_t;

static size_t aquarium_count = 0;
static size_t image_id = 0;

static int create_step(size_t data_count, void** data) {
	for (size_t i = 0; i < data_count; i++) {
		aquarium_state_t* const state = data[i];
		size_t id = aquarium_count++;

		// Generate the cookie.

		asprintf(&state->cookie, "%s/bob/aquarium.cookie.%zu.aquarium", out_path, id);
		assert(state->cookie != NULL);

		// If the cookie already exists, remove it.

		if (access(state->cookie, F_OK) == 0) {
			remove(state->cookie);
		}

		// Create the aquarium.

		LOG_INFO("%s" CLEAR ": Creating aquarium...", state->template);

		cmd_t CMD_CLEANUP cmd = {0};
		cmd_create(&cmd, "aquarium", "-t", state->template, NULL);

		if (state->kernel != NULL) {
			cmd_add(&cmd, "-k");
			cmd_add(&cmd, state->kernel);
		}

		cmd_add(&cmd, "create");
		cmd_add(&cmd, state->cookie);

		cmd_set_redirect(&cmd, false); // So we can get progress if a template needs to be downloaded e.g.
		int const rv = cmd_exec(&cmd);

		if (rv == 0) {
			set_owner(state->cookie);
		}

		cmd_log(&cmd, NULL, state->template, "create aquarium", "created aquarium", true);

		if (rv < 0) {
			return -1;
		}
	}

	return 0;
}

static int exec_step(size_t data_count, void** data) {
	for (size_t i = 0; i < data_count; i++) {
		exec_bss_t* const bss = data[i];

		LOG_INFO("%s" CLEAR ": Executing command on aquarium...", bss->state->template);

		cmd_t CMD_CLEANUP cmd = {0};
		cmd_create(&cmd, "aquarium", "enter", bss->state->cookie, NULL);

		asprintf(&cmd.pending_stdin, "export HOME=/root\n%s\n", bss->cmd);
		assert(cmd.pending_stdin != NULL);

		cmd_set_redirect(&cmd, false); // It's nice to see these logs.
		cmd_exec(&cmd);                // XXX Return values don't matter to us here, commands can fail.

		cmd_log(&cmd, NULL, bss->state->template, "execute command on aquarium", "executed command on aquarium", true);
	}

	return 0;
}

static int image_step(size_t data_count, void** data) {
	for (size_t i = 0; i < data_count; i++) {
		image_bss_t* const bss = data[i];

		LOG_INFO("%s" CLEAR ": Creating bootable image from aquarium...", bss->state->template);

		cmd_t CMD_CLEANUP cmd = {0};
		cmd_create(&cmd, "aquarium", "image", bss->state->cookie, bss->cookie, NULL);
		cmd_set_redirect(&cmd, false); // It's nice to see these logs.
		int rv = cmd_exec(&cmd);

		if (rv == 0) {
			set_owner(bss->cookie);
		}

		cmd_log(&cmd, NULL, bss->state->template, "create bootable image from aquarium", "created bootable image from aquarium", true);

		if (rv < 0) {
			return -1;
		}
	}

	return 0;
}

static int add_kernel(aquarium_state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 1);

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM ": Expected argument to be a string");
		return -1;
	}

	state->kernel = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(state->kernel != NULL);

	return 0;
}

static size_t exec_count = 0;

static int prep_exec(aquarium_state_t* state, flamingo_arg_list_t* args) {
	assert(args->count == 1);
	flamingo_val_t* const arg = args->args[0];

	if (arg->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM ": Expected argument to be string");
		return -1;
	}

	char* const cmd = strndup(arg->str.str, arg->str.size);

	// Add build step to execute on the aquarium.
	// Order absolutely does matter here.

	exec_bss_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;
	bss->cmd = cmd;

	return add_build_step(MAGIC ^ strhash(__func__) + exec_count++, "Executing command on aquarium", exec_step, bss);
}

static int prep_image(aquarium_state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 0);

	image_bss_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;

	asprintf(&bss->cookie, "%s/bob/aquarium.cookie.%zu.img", out_path, image_id++);
	assert(bss->cookie != NULL);

	*rv = flamingo_val_make_cstr(bss->cookie);

	return add_build_step(MAGIC ^ strhash(__func__), "Imaging aquarium", image_step, bss);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	aquarium_state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "add_kernel", callable->name_size) == 0) {
		return add_kernel(state, args, rv);
	}

	else if (flamingo_cstrcmp(callable->name, "exec", callable->name_size) == 0) {
		return prep_exec(state, args);
	}

	else if (flamingo_cstrcmp(callable->name, "image", callable->name_size) == 0) {
		return prep_image(state, args, rv);
	}

	*consumed = false;
	return 0;
}

static void free_state(flamingo_val_t* inst, void* data) {
	aquarium_state_t* const state = data;

	free(state->template);
	free(state->kernel);

	free(state);
}

static int instantiate(flamingo_val_t* inst, flamingo_arg_list_t* args) {
	assert(args->count == 1);

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM ": Expected argument to be a string");
		return -1;
	}

	// Create state object.

	aquarium_state_t* const state = malloc(sizeof *state);
	assert(state != NULL);

	state->template = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(state->template != NULL);

	state->kernel = NULL;

	inst->inst.data = state;
	inst->inst.free_data = free_state;

	// Add build step to create aquarium itself.

	return add_build_step(MAGIC, "Creating aquarium", create_step, state);
}

bob_class_t BOB_CLASS_AQUARIUM = {
	.name = AQUARIUM,
	.populate = NULL,
	.call = call,
	.instantiate = instantiate,
};
