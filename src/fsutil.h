// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdbool.h>
#include <sys/types.h>

int rm(char const* path, char** err);
int copy(char const* src, char const* dst, char** err);
int set_owner(char const* path);
int mkdir_wrapped(char const* path, mode_t mode);
int mkdir_recursive(char const* path, mode_t mode);
char* realpath_better(char const* path);
