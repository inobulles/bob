// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

/**
 * Check if compiler flags have changed since last build.
 *
 * Compares current flags against the saved flags file at '{out}.flags'.
 * If different (or file missing), writes new flags and returns true.
 *
 * @param flags Compiler flags to check.
 * @param out Output artifact path (used to derive the flags file path).
 * @return True if flags changed (rebuild needed), false otherwise.
 */
bool frugal_flags(flamingo_val_t* flags, char* out);

/**
 * Check if any dependency is newer than the target.
 *
 * This uses the mtime (modification time) of the dependencies and the target.
 *
 * @param do_work Set to true if target needs to be rebuilt, false otherwise.
 * @param prefix Log prefix for error messages.
 * @param dep_count Number of dependency paths.
 * @param deps Array of dependency paths.
 * @param target Output artifact path to check against.
 * @return 0 on success, -1 on error.
 */
int frugal_mtime(bool* do_work, char const prefix[static 1], size_t dep_count, char* const* deps, char* target);
