// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <apple.h>
#include <common.h>
#include <frugal.h>
#include <fsutil.h>
#include <install.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>

static flamingo_val_t* install_map = NULL;
static char const* prefix;

int setup_install_map(flamingo_t* flamingo, char const* _prefix) {
	// Find the install map.

	flamingo_scope_t* const scope = flamingo->env->scope_stack[0];

	install_map = NULL;
	flamingo_var_t* map = NULL;

	for (size_t i = 0; i < scope->vars_size; i++) {
		map = &scope->vars[i];

		if (flamingo_cstrcmp(map->key, "install", map->key_size) != 0) {
			continue;
		}

		if (map->val->kind == FLAMINGO_VAL_KIND_NONE) {
			LOG_WARN("Install map not set; nothing to install!");
			return 0;
		}

		if (map->val->kind != FLAMINGO_VAL_KIND_MAP) {
			LOG_FATAL("Install map must be a map.");
			return -1;
		}

		goto found;
	}

	LOG_FATAL("Install map was never declared. This is a serious internal issue, please report it!");
	return -1;

found:

	if (map->val->map.count == 0) {
		LOG_WARN("Install map is empty; nothing to install!");
		return 0;
	}

	// Validate install map entries.

	for (size_t i = 0; i < map->val->map.count; i++) {
		flamingo_val_t* const key_val = map->val->map.keys[i];
		flamingo_val_t* const val_val = map->val->map.vals[i];

		if (key_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Install map key must be a string.");
			return -1;
		}

		if (val_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Install map value must be a string.");
			return -1;
		}
	}

	install_map = map->val;
	prefix = _prefix;

	return 0;
}

static int install_single(flamingo_val_t* key_val, char* val, bool installing_cookie) {
	// Get absolute path of source.

	char* const CLEANUP_STR key = strndup(key_val->str.str, key_val->str.size);
	char* const CLEANUP_STR path = realpath(key, NULL);

	if (path == NULL) {
		assert(errno != ENOMEM);
		LOG_FATAL("Couldn't find source file (from install map): %s", key_val->str.str);
		return -1;
	}

	// If is cookie but we're not installing cookies, skip.
	// When installing a cookie, we're preinstalling, so if the file isn't a cookie something has gone seriously wrong.

	bool const is_cookie = (strstr(path, abs_out_path) == path);

	if (is_cookie && !installing_cookie) {
		return 0;
	}

	assert(is_cookie || !installing_cookie);
	assert((is_cookie ^ installing_cookie) == 0);

	// Make sure destination directory exists.
	// XXX 'dirname' uses internal storage on some platforms, but it seems that with glibc it uses its argument as backing instead.

	char* CLEANUP_STR parent_backing = strdup(val);
	char* parent = dirname(parent_backing);

	char* bit;
	char* CLEANUP_STR accum = strdup(prefix);

	while ((bit = strsep(&parent, "/"))) {
		if (bit[0] == '\0') {
			continue;
		}

		char* CLEANUP_STR path = NULL;
		asprintf(&path, "%s/%s", accum, bit);
		assert(path != NULL);

		if (mkdir_wrapped(path, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", path, strerror(errno));
			return -1;
		}

		free(accum);
		accum = strdup(path);
		assert(accum != NULL);
	}

	// Check modification times.
	// TODO When 'key' is a directory, we should recursively check all files in it.

	char* CLEANUP_STR install_path = NULL;
	asprintf(&install_path, "%s/%s", prefix, val);
	assert(install_path != NULL);

	bool do_install = false;

	if (frugal_mtime(&do_install, "install", 1, &key, install_path) < 0) {
		return -1;
	}

	if (!do_install) {
		LOG_SUCCESS("%s" CLEAR ": Already %sinstalled.", val, is_cookie ? "pre" : "");

		return 0;
	}

	// Actually copy over files.

	if (is_cookie) {
		LOG_INFO("%s" CLEAR ": Preinstalling from cookie...", val);
	}

	else {
		LOG_INFO("%s" CLEAR ": Installing from '%s'...", val, key);
	}

	char* CLEANUP_STR err = NULL;

	if (copy(key, install_path, &err) < 0) {
		LOG_FATAL("Failed to copy '%s' to '%s': %s", key, install_path, err);
		return -1;
	}

#if defined(__APPLE__)
	free(err);
	err = NULL;

	size_t const key_len = strlen(key);
	bool const dylib = key_len >= 2 && strcmp(key + key_len - 2, ".l") == 0;

	if (dylib && apple_set_install_id(install_path, val, &err) < 0) {
		LOG_WARN("Failed to set Apple install ID for dylib '%s': %s", install_path, err);
	}
#endif

	LOG_SUCCESS("%s" CLEAR ": Successfully %sinstalled.", val, is_cookie ? "pre" : "");

	return 0;
}

int install_all(char const* _prefix) {
	assert(_prefix == prefix);

	if (install_map == NULL || prefix == NULL) {
		return 0;
	}

	for (size_t i = 0; i < install_map->map.count; i++) {
		flamingo_val_t* const val_val = install_map->map.vals[i];
		char* const val = strndup(val_val->str.str, val_val->str.size);

		if (install_single(install_map->map.keys[i], val, false) < 0) {
			return -1;
		}
	}

	return 0;
}

char* cookie_to_output(char* cookie, flamingo_val_t** key_val_ref) {
	if (install_map == NULL) {
		return 0;
	}

	for (size_t i = 0; i < install_map->map.count; i++) {
		// Find cookie in install map.

		flamingo_val_t* const key_val = install_map->map.keys[i];

		if (flamingo_cstrcmp(key_val->str.str, cookie, key_val->str.size) != 0) {
			continue;
		}

		// Found; install it.

		if (key_val_ref != NULL) {
			*key_val_ref = key_val;
		}

		flamingo_val_t* const val_val = install_map->map.vals[i];
		char* const val = strndup(val_val->str.str, val_val->str.size);

		assert(val != NULL);
		return val;
	}

	return NULL;
}

int install_cookie(char* cookie) {
	if (prefix == NULL) {
		return 0;
	}

	flamingo_val_t* key = NULL;
	char* const CLEANUP_STR out = cookie_to_output(cookie, &key);

	if (out != NULL && install_single(key, out, true) < 0) {
		return -1;
	}

	return 0;
}
