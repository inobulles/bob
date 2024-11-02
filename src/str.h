// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdint.h>

uint64_t str_hash(char const* str);
void str_free(char* const* str_ref);

#define CLEANUP_STR __attribute__((cleanup(str_free)))
