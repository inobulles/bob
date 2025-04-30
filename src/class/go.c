// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Aymeric Wibo

#include <common.h>

#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <install.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <inttypes.h>

#define GO "Go"

static flamingo_val_t* build_val = NULL;

static void set_env(char const* key, char const* flag, char const* subdir) {
	char* STR_CLEANUP val = getenv(key);
	asprintf(&val, "%s %s%s/%s", val ? val : "", flag, install_prefix, subdir);
	assert(val != NULL);
	setenv(key, val, true);
}

static int build_step(size_t data_count, void** data) {
	if (data_count != 1) {
		LOG_FATAL(GO ".build can't be called more than once (was called %zu times).", data_count);
		return -1;
	}

	// Set up environment for cgo to know where to find the headers and libraries.

	set_env("CGO_CFLAGS", "-I", "include");
	set_env("CGO_LDFLAGS", "-L", "lib");

	// Run "go build".

	LOG_INFO(GO ": Building...");

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "go", "build", "-o", NULL);
	cmd_addf(&cmd, "%s/go.build.cookie.exe", bsys_out_path);

	int rv = cmd_exec(&cmd);
	cmd_log(&cmd, NULL, "Go project", "build", "built", true);

	// Install.

	if (rv == 0) {
		rv = install_cookie(cmd.args[3], true);
	}

	return rv;
}

static int build(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	if (args->count != 0) {
		LOG_FATAL(GO ".build: Didn't expect any arguments, got %zu.", args->count);
		return -1;
	}

	// Check for go executable.

	if (!cmd_exists("go")) {
		LOG_FATAL(GO ".build: Couldn't find 'go' executable in PATH. Go is something you must install separately.");
		return -1;
	}

	// Return output directory.

	char* STR_CLEANUP cookie = NULL;
	asprintf(&cookie, "%s/go.build.cookie.exe", bsys_out_path);
	assert(cookie != NULL);
	*rv = flamingo_val_make_cstr(cookie);

	// Add build step.

	return add_build_step(strhash(GO), "Go build", build_step, NULL);
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

bob_class_t BOB_CLASS_GO = {
	.name = GO,
	.populate = populate,
	.call = call,
};
