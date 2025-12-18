// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

/*
 * Variables.
 *
 * A variable is a named binding to a value within a scope.
 * It consists of a key (the variable name) and a pointer to a {@link flamingo_val_t}.
 *
 * When a value is assigned to a variable, the value's internal name is updated to match the variable's key for better error reporting and debugging.
 */

#pragma once

#include "../common.h"

static void var_set_val(flamingo_var_t* var, flamingo_val_t* val) {
	var->val = val;

	if (val != NULL) {
		if (val->name != NULL) {
			free(val->name);
		}

		val->name = strndup(var->key, var->key_size);
		assert(val->name != NULL);
		val->name_size = var->key_size;
	}
}
