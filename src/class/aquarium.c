// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <inttypes.h>

#define AQUARIUM "Aquarium"
#define MAGIC (strhash(AQUARIUM "() magic"))

typedef struct {
	char* template;
	char* kernel;
} state_t;

static size_t aquarium_count = 0;

static int create_step(size_t data_count, void** data) {
	for (size_t i = 0; i < data_count; i++) {
		state_t* const state = data[i];
		size_t id = aquarium_count++;

		// Generate the cookie.

		char* STR_CLEANUP cookie = NULL;
		asprintf(&cookie, "%s/bob/aquarium.cookie.%zu.aquarium", out_path, id);
		assert(cookie != NULL);

		// Create the aquarium.

		LOG_INFO("%s" CLEAR ": Creating aquarium...", state->template);

		cmd_t CMD_CLEANUP cmd = {0};
		cmd_create(&cmd, "aquarium", "-t", state->template, NULL);

		if (state->kernel != NULL) {
			cmd_add(&cmd, "-k");
			cmd_add(&cmd, state->kernel);
		}

		cmd_add(&cmd, "create");
		cmd_add(&cmd, cookie);

		cmd_set_redirect(&cmd, false); // So we can get progress if a template needs to be downloaded e.g.
		int rv = cmd_exec(&cmd);

		if (rv == 0) {
			set_owner(cookie);
		}

		cmd_log(&cmd, cookie, state->template, "create aquarium", "created aquarium", true);

		if (rv != 0) {
			return -1;
		}
	}

	return 0;
}

static int add_kernel(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 1);

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM ": Expected argument to be a string");
		return -1;
	}

	state->kernel = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(state->kernel != NULL);

	return 0;
}

static int prep_image(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 0);

	// TODO Imaging build step.

	return 0;
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "add_kernel", callable->name_size) == 0) {
		return add_kernel(state, args, rv);
	}

	else if (flamingo_cstrcmp(callable->name, "image", callable->name_size) == 0) {
		return prep_image(state, args, rv);
	}

	*consumed = false;
	return 0;
}

static void free_state(flamingo_val_t* inst, void* data) {
	state_t* const state = data;

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

	state_t* const state = malloc(sizeof *state);
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
