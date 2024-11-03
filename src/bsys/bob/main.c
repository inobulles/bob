// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <bsys.h>
#include <common.h>
#include <build_step.h>
#include <logging.h>
#include <str.h>

#include <class/class.h>
#include <flamingo/flamingo.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#define BUILD_PATH "build.fl"
#define BOB_IMPORT_PATH "/Users/aymeric/bob/bob"

bool consistent = false;

char* src;
size_t src_size;

flamingo_t flamingo;

static bool identify(void) {
	return access(BUILD_PATH, F_OK) != -1;
}

static int external_fn_cb(flamingo_t* flamingo, flamingo_val_t* callable, void* data, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	char* const name = callable->name;
	size_t const name_size = callable->name_size;

	flamingo_val_t* const owner = callable->owner->owner;

	bool const is_static = owner != NULL && owner->kind == FLAMINGO_VAL_KIND_FN && owner->fn.kind == FLAMINGO_FN_KIND_CLASS;
	bool const is_inst = owner != NULL && owner->kind == FLAMINGO_VAL_KIND_INST;

	for (size_t i = 0; i < sizeof(BOB_CLASSES) / sizeof(BOB_CLASSES[0]); i++) {
		bob_class_t const* const bob_class = BOB_CLASSES[i];

		if (is_static && owner != bob_class->class_val) {
			continue;
		}

		if (is_inst && owner->inst.class != bob_class->class_val) {
			continue;
		}

		bool consumed = false;

		if (bob_class->call(callable, args, rv, &consumed) < 0) {
			return -1;
		}

		if (consumed) {
			return 0;
		}
	}

	return flamingo_raise_error(flamingo, "Bob doesn't support the '%.*s' external function call (%zu arguments passed)", (int) name_size, name, args->count);
}

static int class_decl_cb(flamingo_t* flamingo, flamingo_val_t* class, void* data) {
	char* const name = class->name;
	size_t const name_size = class->name_size;
	flamingo_scope_t* const scope = class->fn.scope;

	for (size_t i = 0; i < sizeof(BOB_CLASSES) / sizeof(BOB_CLASSES[0]); i++) {
		bob_class_t* const bob_class = BOB_CLASSES[i];

		if (flamingo_cstrcmp(name, bob_class->name, name_size) != 0) {
			continue;
		}

		bob_class->class_val = class;

		for (size_t j = 0; j < scope->vars_size; j++) {
			flamingo_var_t* const var = &scope->vars[j];

			if (bob_class->populate != NULL) {
				bob_class->populate(var->key, var->key_size, var->val);
			}
		}
	}

	return 0;
}

static int class_inst_cb(flamingo_t* flamingo, flamingo_val_t* inst, void* data, flamingo_arg_list_t* args) {
	flamingo_val_t* const class = inst->inst.class;
	char* const name = class->name;
	size_t const name_size = class->name_size;

	for (size_t i = 0; i < sizeof(BOB_CLASSES) / sizeof(BOB_CLASSES[0]); i++) {
		bob_class_t const* const bob_class = BOB_CLASSES[i];

		if (bob_class->instantiate == NULL) {
			continue;
		}

		if (flamingo_cstrcmp(name, bob_class->name, name_size) != 0) {
			continue;
		}

		if (bob_class->instantiate(inst, args) < 0) {
			return -1;
		}

		break;
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
	flamingo_register_class_inst_cb(&flamingo, class_inst_cb, NULL);

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
	return run_build_steps();
}

static int install(char const* prefix) {
	// Find install map.

	flamingo_scope_t* const scope = flamingo.env->scope_stack[0];
	flamingo_var_t* map = NULL;

	for (size_t i = 0; i < scope->vars_size; i++) {
		map = &scope->vars[i];

		if (flamingo_cstrcmp(map->key, "install", map->key_size) != 0) {
			continue;
		}

		if (map->val->kind == FLAMINGO_VAL_KIND_NONE) {
			LOG_WARN("Install map not set; nothing to install!");
			return 0;
		}

		if (map->val->kind != FLAMINGO_VAL_KIND_MAP) {
			LOG_FATAL("Install map must be a map");
			return -1;
		}

		goto found;
	}

	LOG_FATAL("Install map was never declared. This is a serious issue, please report it!");
	return -1;

found:

	// Go through install map.

	for (size_t i = 0; i < map->val->map.count; i++) {
		// Input validation.

		flamingo_val_t* const key_val = map->val->map.keys[i];
		flamingo_val_t* const val_val = map->val->map.vals[i];

		if (key_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Install map key must be a string");
			return -1;
		}

		if (val_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Install map value must be a string");
			return -1;
		}

		// Get absolute path of source.

		char* const CLEANUP_STR key = strndup(key_val->str.str, key_val->str.size);
		char* const CLEANUP_STR path = realpath(key, NULL);

		if (path == NULL) {
			assert(errno != ENOMEM);
			LOG_FATAL("Couldn't find source file (from install map): %s", key_val->str.str);
			return -1;
		}

		// Figure out if source is a cookie or a regular path.

		bool const is_cookie = (strstr(path, abs_out_path) == path);
		(void) is_cookie;

		printf("%d Installing %s to %s\n", is_cookie, path, prefix);
	}

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
	.key = "bob",
	.identify = identify,
	.setup = setup,
	.build = build,
	.install = install,
	.destroy = destroy,
};
