// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo
// Copyright (c) 2025 Drake Fletcher

#pragma once

#if __linux__
# define _GNU_SOURCE
#endif

#include "flamingo.h"
#include "runtime/tree_sitter/api.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// Grammar parsing prototypes.
//
// These functions are used internally by the interpreter to parse and execute various parts of the Flamingo grammar.
// They typically take a Tree-sitter node and perform the corresponding action.

static inline int parse_vec(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_map(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val_ref);
static inline int parse_unary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_binary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int access_find_var(flamingo_t* flamingo, TSNode node, flamingo_var_t** var, flamingo_val_t** accessed_val);
static inline int parse_access(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val);
static inline int parse_index(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t*** slot, bool lhs);
static inline int parse_statement(flamingo_t* flamingo, TSNode node);
static inline int parse_block(flamingo_t* flamingo, TSNode node, flamingo_scope_t** inner_scope);
static inline int parse_print(flamingo_t* flamingo, TSNode node);
static inline int parse_return(flamingo_t* flamingo, TSNode node);
static inline int parse_break(flamingo_t* flamingo);
static inline int parse_continue(flamingo_t* flamingo);
static inline int parse_assert(flamingo_t* flamingo, TSNode node);
static inline int parse_literal(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_identifier(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_self(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_call(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_var_decl(flamingo_t* flamingo, TSNode node);
static inline int parse_assignment(flamingo_t* flamingo, TSNode node);
static inline int parse_import(flamingo_t* flamingo, TSNode node);
static inline int parse_function_declaration(flamingo_t* flamingo, TSNode node, flamingo_fn_kind_t kind);
static inline int parse_lambda(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_if_chain(flamingo_t* flamingo, TSNode node);
static inline int parse_for_loop(flamingo_t* flamingo, TSNode node);

static inline bool check_is_static(flamingo_t* flamingo, TSNode node);
static inline int find_static_members_in_class(flamingo_t* flamingo, flamingo_scope_t* scope, TSNode body);

// Environment prototypes.

/**
 * Allocate a new environment.
 *
 * @return The new environment.
 */
static inline flamingo_env_t* env_alloc(void);

/**
 * Free an environment.
 *
 * This frees the environment and decrements the reference count of all scopes in its stack.
 *
 * @param env The environment to free.
 */
static inline void env_free(flamingo_env_t* env);

/**
 * Create a closure over an environment.
 *
 * This creates a new environment that shares the scopes of the given environment.
 * The reference counts of the scopes are incremented.
 *
 * @param env The environment to close over.
 * @return The new environment.
 */
static inline flamingo_env_t* env_close_over(flamingo_env_t* env);

/**
 * Get the parent scope of the current scope in the environment.
 *
 * @param env The environment.
 * @return The parent scope, or NULL if there is no parent scope (stack size < 2).
 */
static inline flamingo_scope_t* env_parent_scope(flamingo_env_t* env);

/**
 * Get the current scope in the environment.
 *
 * @param env The environment.
 * @return The current scope.
 */
static inline flamingo_scope_t* env_cur_scope(flamingo_env_t* env);

/**
 * Attach a scope to the environment's stack.
 *
 * This pushes the scope onto the stack.
 * Unlike {@link env_push_scope}, this does NOT allocate a new scope or change the reference count of the provided scope.
 *
 * This is useful when you want to restore a previously detached scope.
 *
 * @param env The environment.
 * @param scope The scope to attach.
 */
static inline void env_gently_attach_scope(flamingo_env_t* env, flamingo_scope_t* scope);

/**
 * Detach the current scope from the environment's stack.
 *
 * This pops the scope from the stack and returns it.
 * Unlike {@link env_pop_scope}, this does NOT decrement the reference count of the scope or free it.
 *
 * This is useful when you want to move a scope elsewhere, for example, when creating a class instance and keeping its scope as the instance's scope.
 *
 * @param env The environment.
 * @return The detached scope.
 */
static inline flamingo_scope_t* env_gently_detach_scope(flamingo_env_t* env);

/**
 * Push a new scope onto the environment's stack.
 *
 * This allocates a new scope and attaches it.
 * The new scope inherits the {@link flamingo_scope_t#class_scope} property from the parent.
 *
 * @param env The environment.
 * @return The new scope.
 */
static inline flamingo_scope_t* env_push_scope(flamingo_env_t* env);

/**
 * Pop the current scope from the environment's stack.
 *
 * This detaches the scope and decrements its reference count.
 *
 * @param env The environment.
 */
static inline void env_pop_scope(flamingo_env_t* env);

/**
 * Find a variable in the environment.
 *
 * This searches the scope stack from top to bottom, allowing for shadowing.
 *
 * @param env The environment.
 * @param key The name of the variable to find.
 * @param key_size The size of the variable name.
 * @return The variable if found, or NULL if not found.
 */
static inline flamingo_var_t* env_find_var(flamingo_env_t* env, char const* key, size_t key_size);

// Scope prototypes.

/**
 * Allocate a new scope.
 *
 * The scope is initialized with a reference count of 1.
 *
 * @return The new scope.
 */
static inline flamingo_scope_t* scope_alloc(void);

/**
 * Free a scope.
 *
 * This frees the scope and all its variables.
 * It should only be called when the reference count reaches 0.
 *
 * @param scope The scope to free.
 */
static inline void scope_free(flamingo_scope_t* scope);

/**
 * Decrement the reference count of a scope.
 *
 * If the reference count reaches 0, the scope is freed.
 *
 * @param scope The scope to decrement the reference count of. Can be NULL.
 */
static inline void scope_decref(flamingo_scope_t* scope);

/**
 * Add a variable to a scope.
 *
 * @param scope The scope to add the variable to.
 * @param key The name of the variable.
 * @param key_size The size of the variable name.
 * @return The new variable.
 */
static inline flamingo_var_t* scope_add_var(flamingo_scope_t* scope, char const* key, size_t key_size);

/**
 * Find a variable in a scope.
 *
 * This only searches the given scope, not any parent scopes.
 * For a search that includes parent scopes, see {@link env_find_var}.
 *
 * @param scope The scope to search.
 * @param key The name of the variable to find.
 * @param key_size The size of the variable name.
 * @return The variable if found, or NULL if not found.
 */
static inline flamingo_var_t* scope_shallow_find_var(flamingo_scope_t* scope, char const* key, size_t key_size);

// Variable prototypes.

/**
 * Set the value of a variable.
 *
 * This also updates the value's name to match the variable's key.
 *
 * @param var The variable to set the value of.
 * @param val The value to set. Can be NULL.
 */
static inline void var_set_val(flamingo_var_t* var, flamingo_val_t* val);

// Value prototypes.

/**
 * Increment the reference count of a value.
 *
 * @param val The value to increment the reference count of.
 * @return The value itself.
 */
static inline flamingo_val_t* val_incref(flamingo_val_t* val);

/**
 * Initialize a value.
 *
 * Sets the value to be an anonymous NONE value with a reference count of 1.
 *
 * @param val The value to initialize.
 * @return The initialized value.
 */
static inline flamingo_val_t* val_init(flamingo_val_t* val);

/**
 * Get the string representation of a value's type.
 *
 * @param val The value to get the type string of.
 * @return The string representation of the value's type (e.g. "integer", "string", "vector").
 */
static inline char const* val_type_str(flamingo_val_t const* val);

/**
 * Get the string representation of a value's role.
 *
 * @param val The value to get the role string of.
 * @return The string representation of the value's role (e.g. "variable", "function", "class").
 */
static inline char const* val_role_str(flamingo_val_t* val);

/**
 * Allocate a new value.
 *
 * The value is initialized with {@link val_init}.
 *
 * @return The new value.
 */
static inline flamingo_val_t* val_alloc(void);

/**
 * Create a copy of a value.
 *
 * For collections (vectors and maps) and instances, this is a shallow copy.
 *
 * @param val The value to copy.
 * @return The new copy of the value.
 */
static inline flamingo_val_t* val_copy(flamingo_val_t* val);

/**
 * Check if two values are equal.
 *
 * This is a deep equality check.
 * For collections, it recursively checks the equality of all elements.
 *
 * Note that this function does NOT check if the names of the values are equal.
 *
 * @param x The first value.
 * @param y The second value.
 * @return True if the values are equal, false otherwise.
 */
static inline bool val_eq(flamingo_val_t* x, flamingo_val_t* y);

/**
 * Free a value.
 *
 * This frees the value and all its contents.
 * It should only be called when the reference count reaches 0.
 *
 * @param val The value to free.
 */
static inline void val_free(flamingo_val_t* val);

/**
 * Decrement the reference count of a value.
 *
 * If the reference count reaches 0, the value is freed.
 *
 * @param val The value to decrement the reference count of. Can be NULL.
 * @return The value itself if it is still alive, or NULL if it was freed (or if val was NULL).
 */
static inline flamingo_val_t* val_decref(flamingo_val_t* val);

// Primitive type member prototypes.

/**
 * Initialize the primitive type member storage.
 *
 * @param flamingo The flamingo instance.
 */
static inline void primitive_type_member_init(flamingo_t* flamingo);

/**
 * Free the primitive type member storage.
 *
 * @param flamingo The flamingo instance.
 */
static inline void primitive_type_member_free(flamingo_t* flamingo);

/**
 * Add a primitive type member.
 *
 * The provided callback is executed when the member is accessed on a value of the specified type (e.g., `my_string.my_member()`).
 *
 * @param flamingo The flamingo instance.
 * @param type The value type to add the member to.
 * @param key_size The size of the member name.
 * @param key The member name.
 * @param cb The callback function for the member.
 * @return 0 on success, -1 on error.
 */
static inline int primitive_type_member_add(flamingo_t* flamingo, flamingo_val_kind_t type, size_t key_size, char* key, flamingo_ptm_cb_t cb);

/**
 * Register standard primitive type members.
 *
 * This registers methods like `len` for strings, etc.
 *
 * @param flamingo The flamingo instance.
 * @return 0 on success, -1 on error.
 */
static inline int primitive_type_member_std(flamingo_t* flamingo);

// Call prototypes.

/**
 * Call a callable value.
 *
 * This function handles setting up the environment for the call (switching to the function's closure environment), pushing a new scope for arguments, and executing the function body or callback.
 *
 * For external functions, {@link flamingo_t#external_fn_cb} must be set.
 *
 * @param flamingo The flamingo instance.
 * @param callable The value to call.
 * @param accessed_val The value on which the callable was accessed (e.g. `obj` in `obj.method()`). Can be NULL.
 * @param rv Output parameter for the return value. Can be NULL.
 * @param args The arguments to pass to the callable.
 * @return 0 on success, -1 on error.
 */
static inline int call(
	flamingo_t* flamingo,
	flamingo_val_t* callable,
	flamingo_val_t* accessed_val,
	flamingo_val_t** rv,
	flamingo_arg_list_t* args
);

/**
 * Get the string representation of a value.
 *
 * This is used for printing values to the console.
 * The returned string is allocated with {@link malloc} and must be freed by the caller.
 *
 * @param flamingo The flamingo instance.
 * @param val The value to get the representation of.
 * @param res Output parameter for the representation string.
 * @return 0 on success, -1 on error.
 */
static inline int repr(flamingo_t* flamingo, flamingo_val_t* val, char** res);

#define error(...) (flamingo_raise_error(__VA_ARGS__))
