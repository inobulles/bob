// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

typedef struct {
	char* name;
	flamingo_val_t* class_val;

	void (*populate)(char* key, size_t key_size, flamingo_val_t* val);
	int (*call)(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed);
	int (*instantiate)(flamingo_val_t* inst, flamingo_arg_list_t* args);
} bob_class_t;

extern bob_class_t BOB_CLASS_AQUARIUM;
extern bob_class_t BOB_CLASS_AQUARIUM_BUILDER;
extern bob_class_t BOB_CLASS_CC;
extern bob_class_t BOB_CLASS_CARGO;
extern bob_class_t BOB_CLASS_FS;
extern bob_class_t BOB_CLASS_GO;
extern bob_class_t BOB_CLASS_LINKER;
extern bob_class_t BOB_CLASS_PKG_CONFIG;
extern bob_class_t BOB_CLASS_PLATFORM;

static bob_class_t* const BOB_CLASSES[] = {
	&BOB_CLASS_AQUARIUM,
	&BOB_CLASS_AQUARIUM_BUILDER,
	&BOB_CLASS_CC,
	&BOB_CLASS_CARGO,
	&BOB_CLASS_FS,
	&BOB_CLASS_GO,
	&BOB_CLASS_LINKER,
	&BOB_CLASS_PKG_CONFIG,
	&BOB_CLASS_PLATFORM,
};
