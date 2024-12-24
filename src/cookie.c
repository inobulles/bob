// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <cookie.h>
#include <str.h>

#include <flamingo/flamingo.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

char* gen_cookie(char* path, size_t path_size, char const* ext) {
	char* cookie = NULL;
	asprintf(&cookie, "%s/bob/%.*s.cookie.%" PRIx64 ".%s", out_path, (int) path_size, path, str_hash(path, path_size), ext);
	assert(cookie != NULL);

	size_t const prefix_len = strlen(out_path) + strlen("/bob/");

	for (size_t i = prefix_len; i < prefix_len + path_size; i++) {
		if (cookie[i] == '/') {
			cookie[i] = '_';
		}
	}

	return cookie;
}

static pthread_mutex_t built_cookies_mutex = PTHREAD_MUTEX_INITIALIZER;
size_t built_cookie_count = 0;
char** built_cookies = NULL; // XXX Shouldn't really worry about freeing all of this, there's no real chance of a leak.

void add_built_cookie(char* cookie) {
	pthread_mutex_lock(&built_cookies_mutex);

	built_cookies = realloc(built_cookies, ++built_cookie_count * sizeof *built_cookies);
	assert(built_cookies != NULL);
	built_cookies[built_cookie_count - 1] = strdup(cookie);

	pthread_mutex_unlock(&built_cookies_mutex);
}

bool has_built_cookie(char* cookie, size_t len) {
	for (size_t i = 0; i < built_cookie_count; i++) {
		if (flamingo_cstrcmp(cookie, built_cookies[i], len) == 0) {
			return true;
		}
	}

	return false;
}
