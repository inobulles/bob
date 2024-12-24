// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

extern size_t built_cookie_count;
extern char** built_cookies;

char* gen_cookie(char* path, size_t path_size, char const* ext);

void add_built_cookie(char* cookie);
bool has_built_cookie(char* cookie, size_t len);
