// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "flamingo.h"
#include "runtime/tree_sitter/api.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Grammar parsing prototypes.

static inline int parse_vec(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_map(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val_ref);
static inline int parse_unary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_binary_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int access_find_var(flamingo_t* flamingo, TSNode node, flamingo_var_t** var, flamingo_val_t** accessed_val);
static inline int parse_access(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val);
static inline int parse_index(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, bool lhs);
static inline int parse_statement(flamingo_t* flamingo, TSNode node);
static inline int parse_block(flamingo_t* flamingo, TSNode node, flamingo_scope_t** inner_scope);
static inline int parse_print(flamingo_t* flamingo, TSNode node);
static inline int parse_return(flamingo_t* flamingo, TSNode node);
static inline int parse_assert(flamingo_t* flamingo, TSNode node);
static inline int parse_literal(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_identifier(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_call(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_var_decl(flamingo_t* flamingo, TSNode node);
static inline int parse_assignment(flamingo_t* flamingo, TSNode node);
static inline int parse_import(flamingo_t* flamingo, TSNode node);
static inline int parse_function_declaration(flamingo_t* flamingo, TSNode node, flamingo_fn_kind_t kind);
static inline int parse_lambda(flamingo_t* flamingo, TSNode node, flamingo_val_t** val);
static inline int parse_if_chain(flamingo_t* flamingo, TSNode node);

static inline bool check_is_static(flamingo_t* flamingo, TSNode node);
static inline int find_static_members_in_class(flamingo_t* flamingo, flamingo_scope_t* scope, TSNode body);

// Environment prototypes.

static inline flamingo_env_t* env_alloc(void);
static inline flamingo_env_t* env_close_over(flamingo_env_t* env);
static inline flamingo_scope_t* env_parent_scope(flamingo_env_t* env);
static inline flamingo_scope_t* env_cur_scope(flamingo_env_t* env);
static inline void env_gently_attach_scope(flamingo_env_t* env, flamingo_scope_t* scope);
static inline flamingo_scope_t* env_gently_detach_scope(flamingo_env_t* env);
static inline flamingo_scope_t* env_push_scope(flamingo_env_t* env);
static inline void env_pop_scope(flamingo_env_t* env);
static inline flamingo_var_t* env_find_var(flamingo_env_t* env, char const* key, size_t key_size);

// Scope prototypes.

static inline flamingo_scope_t* scope_alloc(void);
static inline void scope_free(flamingo_scope_t* scope);
static inline flamingo_var_t* scope_add_var(flamingo_scope_t* scope, char const* key, size_t key_size);
static inline flamingo_var_t* scope_shallow_find_var(flamingo_scope_t* scope, char const* key, size_t key_size);

// Variable prototypes.

static inline void var_set_val(flamingo_var_t* var, flamingo_val_t* val);

// Value prototypes.

static inline flamingo_val_t* val_incref(flamingo_val_t* val);
static inline flamingo_val_t* val_init(flamingo_val_t* val);
static inline char const* val_type_str(flamingo_val_t const* val);
static inline char const* val_role_str(flamingo_val_t* val);
static inline flamingo_val_t* val_alloc(void);
static inline flamingo_val_t* val_copy(flamingo_val_t* val);
static inline bool val_eq(flamingo_val_t* x, flamingo_val_t* y);
static inline void val_free(flamingo_val_t* val);
static inline flamingo_val_t* val_decref(flamingo_val_t* val);

// Primitive type member prototypes.

static inline void primitive_type_member_init(flamingo_t* flamingo);
static inline void primitive_type_member_free(flamingo_t* flamingo);
static inline int primitive_type_member_add(flamingo_t* flamingo, flamingo_val_kind_t type, size_t key_size, char* key, flamingo_ptm_cb_t cb);
static inline int primitive_type_member_std(flamingo_t* flamingo);

// Call prototypes.

static inline int call(
	flamingo_t* flamingo,
	flamingo_val_t* callable,
	flamingo_val_t* accessed_val,
	flamingo_val_t** rv,
	flamingo_arg_list_t* args
);

// Representation prototypes.

static inline int repr(flamingo_t* flamingo, flamingo_val_t* val, char** res);

#define error(...) (flamingo_raise_error(__VA_ARGS__))
