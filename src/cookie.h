// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>

static inline char* gen_cookie(char* path, size_t path_size) {
	char* cookie = NULL;
	asprintf(&cookie, "%.*s.cookie.%llx", (int) path_size, path, str_hash(path));
	assert(cookie != NULL);

	for (size_t i = 0; i < path_size; i++) {
		if (cookie[i] == '/') {
			cookie[i] = '_';
		}
	}

	return cookie;
}
