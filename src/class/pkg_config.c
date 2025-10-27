// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Aymeric Wibo

#include <common.h>

#include <class/class.h>
#include <cmd.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <string.h>

#define PKG_CONFIG "PkgConfig"

static flamingo_val_t* cflags_val = NULL;
static flamingo_val_t* libs_val = NULL;

static int common(
	char const* fn,
	char const* flag,
	flamingo_arg_list_t* args,
	flamingo_val_t** rv
) {
	// Validate arguments.

	if (args->count != 1) {
		LOG_FATAL(PKG_CONFIG ".%s: Expected 1 argument, got %zu.", fn, args->count);
		return -1;
	}

	flamingo_val_t* const arg = args->args[0];

	if (arg->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(PKG_CONFIG ".%s: Expected module argument to be a string.", fn);
		return -1;
	}

	char* STR_CLEANUP module = strndup(arg->str.str, arg->str.size);
	assert(module != NULL);

	// Check if pkg-config is available.

	if (!cmd_exists("pkg-config")) {
		LOG_FATAL(PKG_CONFIG ": Couldn't find 'pkg-config' executable in PATH. pkg-config is something you must install separately.");
		return -1;
	}

	// Run command.

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "pkg-config", flag, module, NULL);
	cmd_set_redirect(&cmd, true, true);

	int const cmd_rv = cmd_exec(&cmd);
	char* STR_CLEANUP orig_out = cmd_read_out(&cmd);

	if (cmd_rv != 0) {
		LOG_ERROR(PKG_CONFIG ".%s: pkg-config failed:", fn, orig_out);
		printf("%s", orig_out);
		return -1;
	}

	// Split output into a vector.

	flamingo_val_t* const vec = flamingo_val_make_none();
	vec->kind = FLAMINGO_VAL_KIND_VEC;

	char* out = orig_out;
	char* tok;

	while ((tok = strsep(&out, " ")) != NULL) {
		// Trim in case last token (which is just before a newline).

		size_t const len = strlen(tok);

		if (tok[len - 1] == '\n') {
			tok[len - 1] = '\0';
		}

		// Add token to vector.

		vec->vec.elems = realloc(vec->vec.elems, (vec->vec.count + 1) * sizeof(flamingo_val_t*));
		assert(vec->vec.elems != NULL);
		vec->vec.elems[vec->vec.count++] = flamingo_val_make_cstr(tok);
	}

	*rv = vec;
	return 0;
}

static int get_cflags(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	return common("cflags", "--cflags", args, rv);
}

static int get_libs(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	return common("libs", "--libs", args, rv);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == cflags_val) {
		return get_cflags(args, rv);
	}

	else if (callable == libs_val) {
		return get_libs(args, rv);
	}

	*consumed = false;
	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "cflags", key_size) == 0) {
		cflags_val = val;
	}

	if (flamingo_cstrcmp(key, "libs", key_size) == 0) {
		libs_val = val;
	}
}

bob_class_t BOB_CLASS_PKG_CONFIG = {
	.name = PKG_CONFIG,
	.populate = populate,
	.call = call,
};
