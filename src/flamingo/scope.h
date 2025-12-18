// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo
// Copyright (c) 2025 Drake Fletcher

/*
 * Scopes.
 *
 * A scope is a container for variables within a specific lexical context.
 * Scopes are reference-counted, as they can be shared between environments (e.g., in the case of closures).
 *
 * Each scope maintains a list of variables ({@link flamingo_var_t}) and can optionally have an "owner" (an instance or class) and a {@link flamingo_scope_t#class_scope} flag to indicate its role in the hierarchy.
 */

#pragma once

#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static flamingo_scope_t* scope_alloc(void) {
	flamingo_scope_t* const scope = malloc(sizeof *scope);
	assert(scope != NULL);

	scope->ref_count = 1;
	scope->vars_size = 0;
	scope->vars = NULL;

	scope->owner = NULL;
	scope->class_scope = false;

	return scope;
}

static void scope_empty(flamingo_scope_t* scope) {
	size_t const count = scope->vars_size;
	flamingo_var_t* const vars = scope->vars;

	scope->vars_size = 0;
	scope->vars = NULL;

	for (size_t i = 0; i < count; i++) {
		flamingo_var_t* const var = &vars[i];

		val_decref(var->val);
		free(var->key);
	}

	free(vars);
}

static void scope_free(flamingo_scope_t* scope) {
	scope_empty(scope);
	free(scope);
}

static void scope_decref(flamingo_scope_t* scope) {
	if (scope == NULL) {
		return;
	}

	assert(scope->ref_count > 0);
	scope->ref_count--;

	if (scope->ref_count == 0) {
		scope_free(scope);
	}
}

static flamingo_var_t* scope_add_var(flamingo_scope_t* scope, char const* key, size_t key_size) {
	scope->vars = (flamingo_var_t*) realloc(scope->vars, ++scope->vars_size * sizeof *scope->vars);
	assert(scope->vars != NULL);

	flamingo_var_t* const var = &scope->vars[scope->vars_size - 1];

	var->key = malloc(key_size);
	assert(var->key != NULL);

	var->key_size = key_size;
	memcpy(var->key, key, key_size);

	var->val = NULL;

	return var;
}

static flamingo_var_t* scope_shallow_find_var(flamingo_scope_t* scope, char const* key, size_t key_size) {
	for (size_t i = 0; i < scope->vars_size; i++) {
		flamingo_var_t* const var = &scope->vars[i];

		if (var->key_size == key_size && memcmp(var->key, key, key_size) == 0) {
			return var;
		}
	}

	return NULL;
}
