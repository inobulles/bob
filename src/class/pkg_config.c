// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Aymeric Wibo

#include <common.h>

#include <alloc.h>
#include <build_step.h>
#include <class/class.h>
#include <cmd.h>
#include <logging.h>
#include <str.h>

#include <string.h>

#include <class/pkg_config.h>

#define PKG_CONFIG "PkgConfig"

typedef struct {
	pkg_config_cookie_t* cookie;
	char* module;
	char const* fn;
	char const* flag;
} bss_t;

static flamingo_val_t* cflags_val = NULL;
static flamingo_val_t* libs_val = NULL;

static int eval(char const* fn, char const* flag, char* module, pkg_config_cookie_t* cookie) {
	// Run command.

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, "pkg-config", flag, module, NULL);
	cmd_set_redirect(&cmd, CMD_REDIRECT, CMD_FORCE_REDIRECT);

	cmd_addf(&cmd, "--with-path=%s/libdata/pkgconfig", default_tmp_install_prefix);
	cmd_addf(&cmd, "--with-path=%s/lib/pkgconfig", default_tmp_install_prefix);

	int const cmd_rv = cmd_exec(&cmd);
	char* orig_out = cmd_read_out(&cmd);

	if (cmd_rv != 0) {
		LOG_ERROR(PKG_CONFIG ".%s: pkg-config failed:", fn, orig_out);
		fprintf(stderr, "%s", orig_out);
		free(module);
		return -1;
	}

	// Split output into a vector.

	flamingo_val_t* const vec = flamingo_val_make_none();
	vec->kind = FLAMINGO_VAL_KIND_VEC;

	char* out = orig_out;
	char* tok;

	while ((tok = strsep(&out, " ")) != NULL) {
		// Trim in case last token (which is just before a newline).

		size_t const len = strlen(tok);

		if (tok[len - 1] == '\n') {
			tok[len - 1] = '\0';
		}

		// Add token to vector.

		vec->vec.elems = realloc_c(vec->vec.elems, (vec->vec.count + 1) * sizeof(flamingo_val_t*));
		vec->vec.elems[vec->vec.count++] = flamingo_val_make_cstr(tok);
	}

	cookie->out_vec = vec;
	free(module);

	return 0;
}

static int eval_step(size_t data_count, void** data) {
	// XXX If we wanted to, I think we could make this a pool without too much difficulty. But cba rn.

	for (size_t i = 0; i < data_count; i++) {
		bss_t* const bss = data[i];

		if (eval(bss->fn, bss->flag, bss->module, bss->cookie) < 0) {
			return -1;
		}
	}

	return 0;
}

static void free_cookie(flamingo_val_t* inst, void* data) {
	pkg_config_cookie_t* const cookie = data;

	if (cookie->out_vec != NULL) {
		flamingo_val_decref(cookie->out_vec);
	}

	free(cookie);
}

static int common(
	char const* fn,
	char const* flag,
	flamingo_arg_list_t* args,
	flamingo_val_t** rv
) {
	// Validate arguments.

	if (args->count != 2) {
		LOG_FATAL(PKG_CONFIG ".%s: Expected 2 arguments, got %zu.", fn, args->count);
		return -1;
	}

	flamingo_val_t* const module = args->args[0];

	if (module->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(PKG_CONFIG ".%s: Expected module argument to be a string.", fn);
		return -1;
	}

	flamingo_val_t* const cookie_val = args->args[1];

	if (cookie_val->kind != FLAMINGO_VAL_KIND_INST) {
		LOG_FATAL(PKG_CONFIG ".%s: Expected cookie argument to be an instance of Cookie.", fn);
		return -1;
	}

	// Check if pkg-config is available.

	if (!cmd_exists("pkg-config")) {
		LOG_FATAL(PKG_CONFIG ": Couldn't find 'pkg-config' executable in PATH. pkg-config is something you must install separately.");
		return -1;
	}

	// Add our data to the cookie.

	pkg_config_cookie_t* const cookie = malloc_c(sizeof *cookie);
	cookie->out_vec = NULL;

	cookie_val->inst.data = cookie;
	cookie_val->inst.free_data = free_cookie;

	// Add build step.
	// It is safe to merge multiple pkg-config evaluation build steps.
	// XXX Technically, don't need to strdup_c() 'fn' and 'flag' cuz will be in .rodata.

	bss_t* const bss = malloc_c(sizeof *bss);

	bss->module = strndup_c(module->str.str, module->str.size);
	bss->cookie = cookie;
	bss->fn = fn;
	bss->flag = flag;

	return add_build_step(((uint64_t) 'pkg-' << 32) | 'conf', "pkg-config evaluation", eval_step, bss);
}

static int get_cflags(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	return common("cflags", "--cflags", args, rv);
}

static int get_libs(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	return common("libs", "--libs", args, rv);
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == cflags_val) {
		return get_cflags(args, rv);
	}

	else if (callable == libs_val) {
		return get_libs(args, rv);
	}

	*consumed = false;
	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "_cflags", key_size) == 0) {
		cflags_val = val;
	}

	if (flamingo_cstrcmp(key, "_libs", key_size) == 0) {
		libs_val = val;
	}
}

bob_class_t BOB_CLASS_PKG_CONFIG = {
	.name = PKG_CONFIG,
	.populate = populate,
	.call = call,
};
