// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

size_t cookie_index = 0;

static inline char* gen_cookie(char* path, size_t path_size) {
	char* cookie = NULL;
	asprintf(&cookie, "%.*s.cookie.%zu", (int) path_size, path, cookie_index++);
	assert(cookie != NULL);

	for (size_t i = 0; i < path_size; i++) {
		if (cookie[i] == '/') {
			cookie[i] = '_';
		}
	}

	return cookie;
}
