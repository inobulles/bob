// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Aymeric Wibo

#include <common.h>

#include <alloc.h>
#include <cookie.h>
#include <str.h>

#include <flamingo/flamingo.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

char* gen_cookie(char* path, size_t path_size, char const* ext) {
	char* cookie = NULL;
	asprintf_c(&cookie, "%s/%.*s.cookie.%" PRIx64 ".%s", bsys_out_path, (int) path_size, path, strnhash(path, path_size), ext);

	size_t const prefix_len = strlen(bsys_out_path) + strlen("/");

	for (size_t i = prefix_len; i < prefix_len + path_size; i++) {
		if (cookie[i] == '/') {
			cookie[i] = '_';
		}
	}

	return cookie;
}

static pthread_mutex_t built_cookies_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t built_cookie_count = 0;
static char** built_cookies = NULL; // XXX Shouldn't really worry about freeing all of this, there's no real chance of a leak.

void add_built_cookie(char* cookie) {
	pthread_mutex_lock(&built_cookies_mutex);

	built_cookies = realloc_c(built_cookies, ++built_cookie_count * sizeof *built_cookies);
	built_cookies[built_cookie_count - 1] = strdup_c(cookie);

	pthread_mutex_unlock(&built_cookies_mutex);
}

bool has_built_cookie(char* cookie, size_t len) {
	bool rv = false;
	pthread_mutex_lock(&built_cookies_mutex);

	for (size_t i = 0; i < built_cookie_count; i++) {
		if (flamingo_cstrcmp(cookie, built_cookies[i], len) == 0) {
			rv = true;
			goto done;
		}
	}

done:

	pthread_mutex_unlock(&built_cookies_mutex);
	return rv;
}
