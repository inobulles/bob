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

#define AQUARIUM_BUILDER "AquariumBuilder"
#define MAGIC (strhash("AquariumBuilder() magic"))

typedef struct {
	char* template;
	char* project_path;

	// This is the aquarium itself.
	// It shouldn't ever be returned anywhere, so it isn't ever copied.

	char* cookie;
} state_t;

typedef struct {
	state_t* state;
} bss_create_t;

static int create_step(size_t data_count, void** data) {
	// TODO Parallelize this, but make sure libaquarium works in parallel first.

	for (size_t i = 0; i < data_count; i++) {
		bss_create_t* const bss = data[i];
		state_t* const state = bss->state;

		// Generate the cookie.

		uint64_t const hash = strhash(state->template) ^ strhash(state->project_path);
		char* STR_CLEANUP cookie = NULL;
		asprintf(&cookie, "%s/bob/aquarium_builder.cookie.%" PRIx64 ".aquarium", out_path, hash);
		assert(cookie != NULL);

		// If it doesn't yet exist, create the aquarium.

		char* STR_CLEANUP pretty = NULL;
		asprintf(&pretty, "Project '%s' with template '%s'", state->template, state->project_path);
		assert(pretty != NULL);

		if (access(cookie, F_OK) == 0) {
			log_already_done(cookie, pretty, "created builder aquarium");
		}

		else {
			LOG_INFO("%s" CLEAR ": Creating builder aquarium, as it doesn't yet exist (this might take several minutes)...", pretty);

			cmd_t CMD_CLEANUP cmd;
			cmd_create(&cmd, "aquarium", "-t", state->template, "create", cookie, NULL);
			int const rv = cmd_exec(&cmd);

			if (rv == 0) {
				set_owner(cookie);
			}

			cmd_log(&cmd, cookie, pretty, "create builder aquarium", "created build aquarium", true);

			// TODO Install Bob.
		}

		// TODO Copy/link over the project.
		// TODO Attempt to build project.
	}

	return 0;
}

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

	state->project_path = strndup(args->args[1]->str.str, args->args[1]->str.size);
	assert(state->project_path != NULL);

	inst->inst.data = state;
	inst->inst.free_data = free_state;

	// Add build step to create aquarium itself.

	bss_create_t* const bss = malloc(sizeof *bss);
	assert(bss != NULL);

	bss->state = state;

	// We can always do this in parallel as we can have no dependencies, so let's always merge these build steps.
	// TODO In fact, I'm pretty sure we can run all the AquariumBuilder creation build steps program-wide. We don't need other build steps to fence this.
	// TODO Is libaquarium happy with this? Either way it's its problem if it breaks in this situation.
	// We also don't check for frugality here, because the project might have changes, so it'll always have to be rebuilt.

	return add_build_step(MAGIC, "Creating aquarium for aquarium builder", create_step, bss);
}

bob_class_t BOB_CLASS_AQUARIUM_BUILDER = {
	.name = AQUARIUM_BUILDER,
	.populate = NULL,
	.call = call,
	.instantiate = instantiate,
};
