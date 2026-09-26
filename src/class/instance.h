// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

// TODO This probably warrants being added as a helper function in Flamingo directly.

#pragma once

#include <flamingo/flamingo.h>

static inline flamingo_val_t* inst_getf(flamingo_val_t* inst, char* key) {
	flamingo_scope_t* const scope = inst->inst.scope;

	for (size_t i = 0; i < scope->vars_size; i++) {
		flamingo_var_t* const var = &scope->vars[i];

		if (flamingo_cstrcmp(var->key, key, var->key_size) == 0) {
			return var->val;
		}
	}

	return NULL;
}

static inline int inst_setf(flamingo_val_t* inst, char* key, flamingo_val_t* val) {
	flamingo_scope_t* const scope = inst->inst.scope;

	for (size_t i = 0; i < scope->vars_size; i++) {
		flamingo_var_t* const var = &scope->vars[i];

		if (flamingo_cstrcmp(var->key, key, var->key_size) == 0) {
			flamingo_val_decref(var->val);
			var->val = val;
			return 0;
		}
	}

	return -1;
}
