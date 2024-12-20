// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "common.h"
#include "scope.h"

#include <assert.h>
#include <stdlib.h>

static flamingo_env_t* env_alloc(void) {
	flamingo_env_t* const env = malloc(sizeof *env);
	assert(env != NULL);

	env->scope_stack_size = 0;
	env->scope_stack = NULL;

	return env;
}

static flamingo_env_t* env_close_over(flamingo_env_t* env) {
	flamingo_env_t* const closed_env = env_alloc();

	closed_env->scope_stack_size = env->scope_stack_size;
	closed_env->scope_stack = malloc(closed_env->scope_stack_size * sizeof *closed_env->scope_stack);
	assert(closed_env->scope_stack != NULL);

	for (size_t i = 0; i < closed_env->scope_stack_size; i++) {
		flamingo_scope_t* const scope = env->scope_stack[i];

		scope->ref_count++; // TODO Prolly wanna use a 'scope_incref' function or something.
		closed_env->scope_stack[i] = scope;
	}

	return closed_env;
}

static flamingo_scope_t* env_parent_scope(flamingo_env_t* env) {
	if (env->scope_stack_size < 2) {
		return NULL;
	}

	return env->scope_stack[env->scope_stack_size - 2];
}

static flamingo_scope_t* env_cur_scope(flamingo_env_t* env) {
	assert(env->scope_stack_size >= 1);
	return env->scope_stack[env->scope_stack_size - 1];
}

static void env_gently_attach_scope(flamingo_env_t* env, flamingo_scope_t* scope) {
	assert(env->scope_stack_size == 0 || env->scope_stack[env->scope_stack_size - 1] != scope);

	env->scope_stack = realloc(env->scope_stack, ++env->scope_stack_size * sizeof *env->scope_stack);
	assert(env->scope_stack != NULL);

	env->scope_stack[env->scope_stack_size - 1] = scope;
}

static flamingo_scope_t* env_gently_detach_scope(flamingo_env_t* env) {
	assert(env->scope_stack_size >= 1);
	return env->scope_stack[--env->scope_stack_size];
}

static flamingo_scope_t* env_push_scope(flamingo_env_t* env) {
	flamingo_scope_t* const scope = scope_alloc();
	env_gently_attach_scope(env, scope);

	flamingo_scope_t* const parent = env_parent_scope(env);
	scope->class_scope = parent != NULL ? parent->class_scope : false;

	return scope;
}

static void env_pop_scope(flamingo_env_t* env) {
	// TODO Free containing variables.
	/* scope_free( */ env_gently_detach_scope(env) /* ) */;
}

static flamingo_var_t* env_find_var(flamingo_env_t* env, char const* key, size_t key_size) {
	// Go backwards down the stack to allow for shadowing.

	for (ssize_t i = env->scope_stack_size - 1; i >= 0; i--) {
		flamingo_scope_t* const scope = env->scope_stack[i];
		flamingo_var_t* const var = scope_shallow_find_var(scope, key, key_size);

		if (var != NULL) {
			return var;
		}
	}

	return NULL;
}
