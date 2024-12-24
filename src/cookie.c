// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <cookie.h>
#include <str.h>

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
