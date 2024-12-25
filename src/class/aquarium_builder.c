// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <class/class.h>
#include <logging.h>
#include <str.h>

#include <assert.h>

#define AQUARIUM_BUILDER "AquariumBuilder"

typedef struct {
	char* template;
	char* project_path;
} state_t;

static int prep_install_to(state_t* state, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 1);
	flamingo_val_t* const arg = args->args[0];

	if (arg->kind != FLAMINGO_VAL_KIND_INST) {
		LOG_FATAL(AQUARIUM_BUILDER ": Expected argument to be an instance of the Aquarium class");
		return -1;
	}

	if (flamingo_cstrcmp(arg->inst.class->name, "Aquarium", arg->inst.class->name_size) != 0) {
		LOG_FATAL(AQUARIUM_BUILDER ": Expected argument to be an instance of the Aquarium class (is instance of '%.*s' instead)", (int) arg->inst.class->name_size, arg->inst.class->name);
		return -1;
	}

	// TODO Installing build step.

	return 0;
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	state_t* const state = callable->owner->owner->inst.data; // TODO Should this be passed to the call function of a class?

	if (flamingo_cstrcmp(callable->name, "install_to", callable->name_size) == 0) {
		return prep_install_to(state, args, rv);
	}

	*consumed = false;
	return 0;
}

static void free_state(flamingo_val_t* inst, void* data) {
	state_t* const state = data;

	free(state->template);
	free(state->project_path);

	free(state);
}

static int instantiate(flamingo_val_t* inst, flamingo_arg_list_t* args) {
	assert(args->count == 2);

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM_BUILDER ": Expected template argument to be a string");
		return -1;
	}

	if (args->args[1]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(AQUARIUM_BUILDER ": Expected project path argument to be a string");
		return -1;
	}

	// Create state object.

	state_t* const state = malloc(sizeof *state);
	assert(state != NULL);

	state->template = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(state->template != NULL);

	state->project_path = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(state->project_path != NULL);

	inst->inst.data = state;
	inst->inst.free_data = free_state;

	// TODO Add build step to create aquarium itself.

	return 0;
}

bob_class_t BOB_CLASS_AQUARIUM_BUILDER = {
	.name = AQUARIUM_BUILDER,
	.populate = NULL,
	.call = call,
	.instantiate = instantiate,
};
