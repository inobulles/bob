// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct flamingo_t flamingo_t;
typedef struct flamingo_val_t flamingo_val_t;
typedef struct flamingo_var_t flamingo_var_t;
typedef struct flamingo_scope_t flamingo_scope_t;
typedef struct flamingo_env_t flamingo_env_t;
typedef struct flamingo_arg_list_t flamingo_arg_list_t;

typedef int (*flamingo_external_fn_cb_t)(flamingo_t* flamingo, size_t name_size, char* name, void* data, flamingo_arg_list_t* args, flamingo_val_t** rv);
typedef int (*flamingo_ptm_cb_t)(flamingo_t* flamingo, flamingo_val_t* self, flamingo_arg_list_t* args, flamingo_val_t** rv);

typedef enum {
	FLAMINGO_VAL_KIND_NONE,
	FLAMINGO_VAL_KIND_BOOL,
	FLAMINGO_VAL_KIND_INT,
	FLAMINGO_VAL_KIND_STR,
	FLAMINGO_VAL_KIND_VEC,
	FLAMINGO_VAL_KIND_MAP,
	FLAMINGO_VAL_KIND_FN,
	FLAMINGO_VAL_KIND_INST,
	FLAMINGO_VAL_KIND_COUNT,
} flamingo_val_kind_t;

typedef enum {
	FLAMINGO_FN_KIND_FUNCTION,
	FLAMINGO_FN_KIND_CLASS,
	FLAMINGO_FN_KIND_EXTERN,
	FLAMINGO_FN_KIND_PTM,
} flamingo_fn_kind_t;

typedef void* flamingo_ts_node_t; // Opaque type, because user shouldn't have to include Tree-sitter stuff in their namespace (or concern themselves with Tree-sitter at all for that matter).

struct flamingo_val_t {
	char* name;
	size_t name_size;

	flamingo_val_kind_t kind;
	size_t ref_count;

	union {
		struct {
			bool boolean;
		} boolean;

		struct {
			int64_t integer;
		} integer;

		struct {
			char* str;
			size_t size;
		} str;

		struct {
			size_t count;
			flamingo_val_t** elems;
		} vec;

		struct {
			size_t count;
			flamingo_val_t** keys;
			flamingo_val_t** vals;
		} map;

		struct {
			flamingo_ts_node_t body;
			flamingo_ts_node_t params;

			// The environment the function closes over.

			flamingo_env_t* env;

			// Functions can be defined in other files entirely.
			// While the Tree-sitter state is held within the nodes themselves, the source they point to is not, which is why we need to keep track of it here.

			char* src;
			size_t src_size;

			// Classes are just functions which can only return instances.

			flamingo_fn_kind_t kind;

			// Only used for primitive type members.

			flamingo_ptm_cb_t ptm_cb;

			// The class' static environment.
			// This works quite similarly to instances.

			flamingo_scope_t* scope;
		} fn;

		struct {
			flamingo_val_t* class;
			flamingo_scope_t* scope;

			// Data managed by the user.

			void* data;
			void (*free_data)(flamingo_val_t* val, void* data);
		} inst;
	};
};

struct flamingo_var_t {
	bool anonymous;
	char* key;
	size_t key_size;
	flamingo_val_t* val;
};

struct flamingo_scope_t {
	size_t ref_count;

	size_t vars_size;
	flamingo_var_t* vars;

	// Used for return to know what it can and can't return.

	bool class_scope;
};

struct flamingo_env_t {
	size_t scope_stack_size;
	flamingo_scope_t** scope_stack;
};

struct flamingo_arg_list_t {
	size_t count;
	flamingo_val_t** args;
};

struct flamingo_t {
	char const* progname;
	bool consistent; // Set if we managed to create the instance, so we know whether or not it still needs freeing.

	char* src;
	size_t src_size;

	char err[256];
	bool errors_outstanding;

	flamingo_external_fn_cb_t external_fn_cb;
	void* external_fn_cb_data;

	// Runtime stuff.

	bool inherited_env;
	flamingo_env_t* env;

	// Tree-sitter stuff.

	void* ts_state;

	// Import-related stuff, i.e. stuff we have to free ourselves.

	size_t import_count;
	flamingo_t* imported_flamingos;
	char** imported_srcs;

	// Import paths for global imports.
	// These shouldn't be inherited by imported instances for now.

	size_t import_path_count;
	char** import_paths;

	// Current function stuff.

	flamingo_ts_node_t cur_fn_body;
	flamingo_val_t* cur_fn_rv;

	// Variables on primitive types.
	// This is for stuff like e.g. '"zonnebloemgranen".endswith("granen")'.

	struct {
		size_t count;
		flamingo_var_t* vars;
	} primitive_type_members[FLAMINGO_VAL_KIND_COUNT];
};

__attribute__((format(printf, 2, 3))) int flamingo_raise_error(flamingo_t* flamingo, char const* fmt, ...);

int flamingo_create(flamingo_t* flamingo, char const* progname, char* src, size_t src_size);
void flamingo_destroy(flamingo_t* flamingo);

char* flamingo_err(flamingo_t* flamingo);
void flamingo_register_external_fn_cb(flamingo_t* flamingo, flamingo_external_fn_cb_t cb, void* data);
void flamingo_add_import_path(flamingo_t* flamingo, char* path);
int flamingo_inherit_env(flamingo_t* flamingo, flamingo_env_t* env);
int flamingo_run(flamingo_t* flamingo);

flamingo_var_t* flamingo_find_var(flamingo_t* flamingo, char const* key, size_t key_size);

flamingo_val_t* flamingo_val_make_none(void);
flamingo_val_t* flamingo_val_make_int(int64_t integer);
flamingo_val_t* flamingo_val_make_str(size_t size, char* str);
flamingo_val_t* flamingo_val_make_cstr(char* str);
flamingo_val_t* flamingo_val_make_bool(bool boolean);

static inline int flamingo_strcmp(char const* a, char const* b, size_t a_size, size_t b_size) {
	if (a_size != b_size) {
		return -1; // XXX Not right but whatever.
	}

	return memcmp(a, b, a_size);
}

static inline int flamingo_cstrcmp(char* str, char* cstr, size_t str_size) {
	return flamingo_strcmp(str, cstr, str_size, strlen(cstr));
}
