// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <common.h>

static void var_set_val(flamingo_var_t* var, flamingo_val_t* val) {
	if (var->val != NULL && val != NULL) {
		val->name = NULL;
		val->name_size = 0;
	}

	var->val = val;

	if (val != NULL) {
		val->name = var->key;
		val->name_size = var->key_size;
	}
}
