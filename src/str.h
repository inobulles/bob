// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdint.h>
#include <stdlib.h>

// TODO Rename to strnhash and have strhash equivalent for C-strings?

uint64_t str_hash(char const* str, size_t len);
void str_free(char* const* str_ref);

#define STR_CLEANUP __attribute__((cleanup(str_free)))
