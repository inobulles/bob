// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <class/class.h>
#include <logging.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>

static flamingo_val_t* compile_val = NULL;

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	// TODO This ain't gonna work cuz 'compile' isn't static.

	if (flamingo_cstrcmp(key, "compile", key_size) == 0) {
		compile_val = val;
	}
}

static int compile(flamingo_arg_list_t* args) {
	printf("TODO compile\n");

	return 0;
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == compile_val) {
		return compile(args);
	}

	*consumed = false;
	return 0;
}

static int instantiate(flamingo_val_t* inst, flamingo_arg_list_t* args) {
	if (args->count != 1) {
		LOG_FATAL("CC: Expected 1 argument, got %zu", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_VEC) {
		LOG_FATAL("CC: Expected argument to be a vector");
		return -1;
	}

	flamingo_val_t* const flags = args->args[0];

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];

		if (flag->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("CC: Expected %zu-th vector element to be a string", i);
			return -1;
		}

		printf("TODO CC flag: %.*s\n", (int) flag->str.size, flag->str.str);
	}

	return 0;
}

bob_class_t const BOB_CLASS_CC = {
	.name = "CC",
	.populate = populate,
	.call = call,
	.instantiate = instantiate,
};
