// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint64_t strnhash(char const* str, size_t len);
void str_free(char* const* str_ref);

static inline uint64_t strhash(char const* str) {
	return strnhash(str, strlen(str));
}

#define STR_CLEANUP __attribute__((cleanup(str_free)))
