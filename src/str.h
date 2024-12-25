// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO Rename to strnhash.

uint64_t str_hash(char const* str, size_t len);
void str_free(char* const* str_ref);

static inline uint64_t strhash(char const* str) {
	return str_hash(str, strlen(str));
}

#define STR_CLEANUP __attribute__((cleanup(str_free)))
