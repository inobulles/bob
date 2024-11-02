// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <stdlib.h>
#include <str.h>
#include <string.h>

uint64_t str_hash(char const* str) { // djb2 algorithm.
	uint64_t hash = 5381;
	size_t const len = strlen(str);

	for (size_t i = 0; i < len; i++) {
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]
	}

	return hash;
}

void str_free(char* const* str_ref) {
	char* const str = *str_ref;

	if (!str) {
		return;
	}

	free(str);
}
