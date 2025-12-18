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

/**
 * Callback for external functions.
 *
 * @param flamingo The flamingo instance.
 * @param callable The callable value being called.
 * @param data User data passed to the callback.
 * @param args The arguments passed to the function.
 * @param rv Output parameter for the return value.
 * @return 0 on success, -1 on error.
 */
typedef int (*flamingo_external_fn_cb_t)(
	flamingo_t* flamingo,
	flamingo_val_t* callable,
	void* data,
	flamingo_arg_list_t* args,
	flamingo_val_t** rv
);

/**
 * Callback for class declarations.
 *
 * @param flamingo The flamingo instance.
 * @param class The class value being declared.
 * @param data User data passed to the callback.
 * @return 0 on success, -1 on error.
 */
typedef int (*flamingo_class_decl_cb_t)(
	flamingo_t* flamingo,
	flamingo_val_t* class,
	void* data
);

/**
 * Callback for class instantiation.
 *
 * @param flamingo The flamingo instance.
 * @param instance The instance value being created.
 * @param data User data passed to the callback.
 * @param args The arguments passed to the constructor.
 * @return 0 on success, -1 on error.
 */
typedef int (*flamingo_class_inst_cb_t)(
	flamingo_t* flamingo,
	flamingo_val_t* instance,
	void* data,
	flamingo_arg_list_t* args
);

/**
 * Callback for primitive type members.
 *
 * @param flamingo The flamingo instance.
 * @param self The value on which the member was accessed.
 * @param args The arguments passed to the member function.
 * @param rv Output parameter for the return value.
 * @return 0 on success, -1 on error.
 */
typedef int (*flamingo_ptm_cb_t)(
	flamingo_t* flamingo,
	flamingo_val_t* self,
	flamingo_arg_list_t* args,
	flamingo_val_t** rv
);

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

	// The scope this value was created in.

	flamingo_scope_t* owner;

	// All the type-specific data.

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
	bool is_static;

	char* key;
	size_t key_size;

	flamingo_val_t* val;
};

struct flamingo_scope_t {
	size_t ref_count;

	size_t vars_size;
	flamingo_var_t* vars;

	// If scope is an instance's scope or a class' static scope, this will be set to that instance or class.

	flamingo_val_t* owner;

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

	// Callbacks.

	flamingo_external_fn_cb_t external_fn_cb;
	void* external_fn_cb_data;

	flamingo_class_decl_cb_t class_decl_cb;
	void* class_decl_cb_data;

	flamingo_class_inst_cb_t class_inst_cb;
	void* class_inst_cb_data;

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

	// Current loop stuff.

	size_t in_loop;
	bool breaking;
	bool continuing;

	// Variables on built-in types.
	// This is for stuff like e.g. '"zonnebloemgranen".endswith("granen")'.

	struct {
		size_t count;
		flamingo_var_t* vars;
	} primitive_type_members[FLAMINGO_VAL_KIND_COUNT];
};

/**
 * Raise an error.
 *
 * This formats the error message and stores it in the flamingo instance.
 * If an error is already outstanding, the new error is appended to the existing one.
 *
 * @param flamingo The flamingo instance.
 * @param fmt The format string (printf-style).
 * @param ... The arguments for the format string.
 * @return Always returns -1.
 */
__attribute__((format(printf, 2, 3))) int flamingo_raise_error(flamingo_t* flamingo, char const* fmt, ...);

/**
 * Create a new flamingo instance.
 *
 * This initializes the flamingo instance and parses the source code.
 * Note that this does NOT set up the runtime environment (scopes), that happens in {@link flamingo_run} or {@link flamingo_inherit_env}.
 * Note the {@link flamingo_t#src} buffer is NOT copied.
 * It must remain valid for the lifetime of the flamingo instance.
 *
 * @param flamingo The flamingo instance to initialize.
 * @param progname The name of the program (used for error messages).
 * @param src The source code to parse.
 * @param src_size The size of the source code.
 * @return 0 on success, -1 on error.
 */
int flamingo_create(flamingo_t* flamingo, char const* progname, char* src, size_t src_size);

/**
 * Destroy a flamingo instance.
 *
 * This frees all resources associated with the flamingo instance, including the AST, environment (if not inherited), and imported modules.
 *
 * @param flamingo The flamingo instance to destroy.
 */
void flamingo_destroy(flamingo_t* flamingo);

/**
 * Get the last error message.
 *
 * This returns the error message stored in the flamingo instance and clears the error state.
 *
 * @param flamingo The flamingo instance.
 * @return The error message, or "no errors" if no error is outstanding.
 */
char* flamingo_err(flamingo_t* flamingo);

/**
 * Register a callback for external functions.
 *
 * This callback is called when an external function is called in the script.
 *
 * @param flamingo The flamingo instance.
 * @param cb The callback function.
 * @param data User data to pass to the callback.
 */
void flamingo_register_external_fn_cb(flamingo_t* flamingo, flamingo_external_fn_cb_t cb, void* data);

/**
 * Register a callback for class declarations.
 *
 * This callback is called when a class is declared in the script.
 *
 * @param flamingo The flamingo instance.
 * @param cb The callback function.
 * @param data User data to pass to the callback.
 */
void flamingo_register_class_decl_cb(flamingo_t* flamingo, flamingo_class_decl_cb_t cb, void* data);

/**
 * Register a callback for class instantiation.
 *
 * This callback is called when a class is instantiated in the script.
 *
 * @param flamingo The flamingo instance.
 * @param cb The callback function.
 * @param data User data to pass to the callback.
 */
void flamingo_register_class_inst_cb(flamingo_t* flamingo, flamingo_class_inst_cb_t cb, void* data);

/**
 * Add an import path.
 *
 * This adds a path to the list of paths searched when importing modules.
 * The path string is duplicated, so the caller retains ownership of the original string.
 *
 * @param flamingo The flamingo instance.
 * @param path The path to add.
 */
void flamingo_add_import_path(flamingo_t* flamingo, char* path);

/**
 * Inherit an environment from another flamingo instance.
 *
 * This allows the flamingo instance to share the environment (variables, scopes) of another instance.
 *
 * @param flamingo The flamingo instance.
 * @param env The environment to inherit.
 * @return 0 on success, -1 on error (e.g. if the instance already has an environment).
 */
int flamingo_inherit_env(flamingo_t* flamingo, flamingo_env_t* env);

/**
 * Run the flamingo script.
 *
 * This executes the parsed script.
 * If the environment is not inherited, any existing environment is freed and a new one is created.
 * This means that subsequent calls to {@link flamingo_run} on the same instance will reset the state.
 *
 * @param flamingo The flamingo instance.
 * @return 0 on success, -1 on error.
 */
int flamingo_run(flamingo_t* flamingo);

/**
 * Find a variable in the current environment.
 *
 * @param flamingo The flamingo instance.
 * @param key The name of the variable.
 * @param key_size The size of the variable name.
 * @return The variable if found, or NULL if not found.
 */
flamingo_var_t* flamingo_find_var(flamingo_t* flamingo, char const* key, size_t key_size);

/**
 * Increment the reference count of a value.
 *
 * This should be used when the external program needs to hold on to the value.
 *
 * @param val The value to increment the reference of.
 * @return The value.
 */
flamingo_val_t* flamingo_val_incref(flamingo_val_t* val);

/**
 * Decrement the reference count of a value.
 *
 * This should be used when the external program needs to let go of a value it previously referenced with {@link flamingo_val_incref} or any value it has created with the `flamingo_val_make_*` family of functions.
 * It is okay to pass NULL to this function.
 *
 * @param val The value to decrement the reference of or NULL.
 * @return The value or NULL if NULL was passed.
 */
flamingo_val_t* flamingo_val_decref(flamingo_val_t* val);

/**
 * Create a NONE value.
 *
 * The returned value has a reference count of 1.
 *
 * @return A new NONE value.
 */
flamingo_val_t* flamingo_val_make_none(void);

/**
 * Create an integer value.
 *
 * The returned value has a reference count of 1.
 *
 * @param integer The integer value.
 * @return A new integer value.
 */
flamingo_val_t* flamingo_val_make_int(int64_t integer);

/**
 * Create a string value.
 *
 * This creates a copy of the given string.
 * Note that the stored string is NOT null-terminated.
 * The returned value has a reference count of 1.
 *
 * @param size The size of the string.
 * @param str The string data.
 * @return A new string value.
 */
flamingo_val_t* flamingo_val_make_str(size_t size, char* str);

/**
 * Create a string value from a C string.
 *
 * This creates a copy of the given null-terminated string.
 * Note that the stored string is NOT null-terminated (it does not include the null terminator).
 * The returned value has a reference count of 1.
 *
 * @param str The C string.
 * @return A new string value.
 */
flamingo_val_t* flamingo_val_make_cstr(char* str);

/**
 * Create a boolean value.
 *
 * The returned value has a reference count of 1.
 *
 * @param boolean The boolean value.
 * @return A new boolean value.
 */
flamingo_val_t* flamingo_val_make_bool(bool boolean);

/**
 * Compare two strings.
 *
 * Note that this function returns -1 if the sizes differ, regardless of the content.
 *
 * @param a The first string.
 * @param b The second string.
 * @param a_size The size of the first string.
 * @param b_size The size of the second string.
 * @return 0 if the strings are equal, non-zero otherwise.
 */
static inline int flamingo_strcmp(char const* a, char const* b, size_t a_size, size_t b_size) {
	if (a_size != b_size) {
		return -1; // XXX Not right but whatever.
	}

	return memcmp(a, b, a_size);
}

/**
 * Compare a string with a C string.
 *
 * @param str The string.
 * @param cstr The C string.
 * @param str_size The size of the string.
 * @return 0 if the strings are equal, non-zero otherwise.
 */
static inline int flamingo_cstrcmp(char* str, char* cstr, size_t str_size) {
	return flamingo_strcmp(str, cstr, str_size, strlen(cstr));
}
