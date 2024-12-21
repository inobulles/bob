// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <class/class.h>
#include <logging.h>

#include <assert.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static flamingo_val_t* os_val = NULL;

static int os(flamingo_val_t** rv) {
	// TODO Get the OS we're on.

	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "os", key_size) == 0) {
		os_val = val;
	}
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == os_val) {
		assert(args->count == 0);
		return os(rv);
	}

	*consumed = false;
	return 0;
}

bob_class_t BOB_CLASS_PLATFORM = {
	.name = "Platform",
	.populate = populate,
	.call = call,
};
