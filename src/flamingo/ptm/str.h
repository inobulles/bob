// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <common.h>
#include <val.h>
#include <var.h>

static inline int str_len(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_STR);

	if (args->count != 0) {
		return error(flamingo, "'str.len' expected 0 arguments, got %zu", args->count);
	}

	*rv = flamingo_val_make_int(self->str.size);
	return 0;
}

static inline int str_startswith(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_STR);

	// Check our arguments.

	if (args->count != 1) {
		return error(flamingo, "'str.startswith' expected 1 argument, got %zu", args->count);
	}

	flamingo_val_t* const start = args->args[0];

	if (start->kind != FLAMINGO_VAL_KIND_STR) {
		return error(flamingo, "'str.startswith' expected 'start' argument to be a string, got a %s", val_role_str(start));
	}

	// Actually check self's string starts with 'start'.

	bool startswith = false;

	if (start->str.size <= self->str.size) {
		startswith = memcmp(self->str.str, start->str.str, start->str.size) == 0;
	}

	*rv = flamingo_val_make_bool(startswith);

	return 0;
}

static inline int str_endswith(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(self->kind == FLAMINGO_VAL_KIND_STR);

	// Check our arguments.

	if (args->count != 1) {
		return error(flamingo, "'str.endswith' expected 1 argument, got %zu", args->count);
	}

	flamingo_val_t* const end = args->args[0];

	if (end->kind != FLAMINGO_VAL_KIND_STR) {
		return error(flamingo, "'str.endswith' expected 'end' argument to be a string, got a %s", val_role_str(end));
	}

	// Actually check self's string ends with 'end'.

	bool endswith = false;

	if (end->str.size <= self->str.size) {
		size_t const delta = self->str.size - end->str.size;
		endswith = memcmp(self->str.str + delta, end->str.str, end->str.size) == 0;
	}

	*rv = flamingo_val_make_bool(endswith);

	return 0;
}
