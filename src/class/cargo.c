// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <logging.h>
#include <str.h>

#define CARGO "Cargo"

static flamingo_val_t* build_val = NULL;

static int build_step(size_t data_count, void** data) {
	if (data_count != 1) {
		LOG_FATAL(CARGO ".build can't be called more than once (was called %zu times).", data_count);
		return -1;
	}

	// Run "cargo build".

	LOG_INFO(CARGO ": Building...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "cargo", "build", NULL);

	int const rv = cmd_exec(&cmd);
	cmd_log(&cmd, NULL, "Cargo project", "build", "built", true);

	return rv;
}

static int build(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	if (args->count != 0) {
		LOG_FATAL(CARGO ".build: Didn't expect any arguments, got %zu.", args->count);
		return -1;
	}

	// Check for cargo executable.

	if (!cmd_exists("cargo")) {
		LOG_FATAL(CARGO ".build: Couldn't find 'cargo' executable in PATH. Cargo is something you must install separately.");
		return -1;
	}

	// Return "target" directory.
	// We don't attempt to put this in '.bob' so we don't interfere with existing tooling (like LSPs and stuff).

	*rv = flamingo_val_make_cstr("target/debug/");

	// Add build step.

	return add_build_step(strhash(CARGO), "Cargo build", build_step, NULL);
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "build", key_size) == 0) {
		build_val = val;
	}
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == build_val) {
		return build(args, rv);
	}

	*consumed = false;
	return 0;
}

bob_class_t BOB_CLASS_CARGO = {
	.name = CARGO,
	.populate = populate,
	.call = call,
};
