// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>
#include <logging.h>

#include <flamingo/flamingo.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUILD_PATH "build.fl"
#define BOB_IMPORT_PATH "/Users/aymeric/bob/bob"

bool consistent = false;

char* src;
size_t src_size;

flamingo_t flamingo;

typedef struct {
	char const* name;
	int (*cb) (void* data);
	void* data;
} build_step_t;

size_t build_step_count;
build_step_t* build_steps;

static bool identify(void) {
	return access(BUILD_PATH, F_OK) != -1;
}

static flamingo_val_t* fs_list = NULL;

static int external_fn_cb(flamingo_t* flamingo, flamingo_val_t* callable, void* data, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	char* const name = callable->name;
	size_t const name_size = callable->name_size;

	if (callable == fs_list) {
		return 0; // TODO
	}

	return flamingo_raise_error(flamingo, "Bob doesn't support the '%.*s' external function call (%zu arguments passed)", (int) name_size, name, args->count);
}

static int class_decl_cb(flamingo_t* flamingo, flamingo_val_t* class, void* data) {
	char* const name = class->name;
	size_t const name_size = class->name_size;
	flamingo_scope_t* const scope = class->fn.scope;

	if (flamingo_cstrcmp(name, "Fs", name_size) == 0) {
		for (size_t i = 0; i < scope->vars_size; i++) {
			flamingo_var_t* const var = &scope->vars[i];

			if (flamingo_cstrcmp(var->key, "list", var->key_size) == 0) {
				fs_list = var->val;
			}
		}
	}

	return 0;
}

static int setup(void) {
	int rv = -1;
	consistent = false;

	// Read build file.

	FILE* const f = fopen(BUILD_PATH, "r");

	if (f == NULL) {
		LOG_FATAL("Failed to open build file: %s", BUILD_PATH);
		goto err_fopen;
	}

	struct stat sb;

	if (fstat(fileno(f), &sb) < 0) {
		LOG_FATAL("Failed to stat build file: %s", BUILD_PATH);
		goto err_fstat;
	}

	src_size = sb.st_size;
	src = malloc(src_size);
	assert(src != NULL);

	if (fread(src, 1, src_size, f) != src_size) {
		LOG_FATAL("Failed to read build file: %s", BUILD_PATH);
		goto err_fread;
	}

	// Create Flamingo engine.

	if (flamingo_create(&flamingo, "Bob the Builder", src, src_size) < 0) {
		LOG_FATAL("Failed to create Flamingo engine: %s", flamingo_err(&flamingo));
		goto err_flamingo_create;
	}

	flamingo_register_external_fn_cb(&flamingo, external_fn_cb, NULL);
	flamingo_register_class_decl_cb(&flamingo, class_decl_cb, NULL);
	flamingo_add_import_path(&flamingo, BOB_IMPORT_PATH);

	// Run build program.

	if (flamingo_run(&flamingo) < 0) {
		LOG_FATAL("Failed to run build program: %s", flamingo_err(&flamingo));
		goto err_flamingo_run;
	}

	// Make sure 'bob.fl' has been imported.

	flamingo_scope_t* const scope = flamingo.env->scope_stack[0];

	for (size_t i = 0; i < scope->vars_size; i++) {
		flamingo_var_t const* const var = &scope->vars[i];

		if (flamingo_cstrcmp(var->key, "__bob_has_been_imported__", var->key_size) == 0) {
			goto found;
		}
	}

	LOG_FATAL("Invalid build file; missing 'bob' import");
	goto err_bad_build_file;

found:

	// Success!

	consistent = true;
	rv = 0;

err_bad_build_file:
err_flamingo_run:

	if (rv != 0) {
		flamingo_destroy(&flamingo);
	}

err_flamingo_create:

	if (rv != 0) {
		free(src);
	}

err_fread:
err_fstat:

	fclose(f);

err_fopen:

	return rv;
}

static int build(void) {
	// TODO Run build steps.

	return 0;
}

static void destroy(void) {
	if (!consistent) {
		return;
	}

	free(src);
	flamingo_destroy(&flamingo);
}

bsys_t const BSYS_BOB = {
	.name = "Bob",
	.identify = identify,
	.setup = setup,
	.build = build,
	.destroy = destroy,
};
