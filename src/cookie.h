// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <str.h>

static inline char* gen_cookie(char* path, size_t path_size, char const* ext) {
	char* cookie = NULL;
	asprintf(&cookie, "%s/bob/%.*s.cookie.%llx.%s", out_path, (int) path_size, path, str_hash(path), ext);
	assert(cookie != NULL);

	size_t const prefix_len = strlen(out_path) + strlen("/bob/");

	for (size_t i = prefix_len; i < prefix_len + path_size; i++) {
		if (cookie[i] == '/') {
			cookie[i] = '_';
		}
	}

	return cookie;
}
