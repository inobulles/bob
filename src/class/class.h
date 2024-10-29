// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

typedef struct {
	char* name;
	void (*populate)(char* key, size_t key_size, flamingo_val_t* val);
	int (*call)(flamingo_val_t* callable, bool* consumed);
} bob_class_t;

extern bob_class_t const BOB_CLASS_FS;

static bob_class_t const* const BOB_CLASSES[] = {
	&BOB_CLASS_FS,
};