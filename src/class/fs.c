// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <class/class.h>

#include <stdio.h>

static flamingo_val_t* list = NULL;

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "list", key_size) == 0) {
		list = val;
	}
}

static int call(flamingo_val_t* callable, bool* consumed) {
	if (callable == list) {
		printf("TODO\n");

		return 0;
	}

	return 0;
}

bob_class_t const BOB_CLASS_FS = {
	.name = "Fs",
	.populate = populate,
	.call = call,
};
