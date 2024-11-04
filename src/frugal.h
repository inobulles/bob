// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

bool frugal_flags(flamingo_val_t* flags, char* out);
int frugal_mtime(bool* do_work, char const prefix[static 1], size_t dep_count, char* const* deps, char* target);
