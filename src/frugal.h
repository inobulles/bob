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
 * @param log_prefix Log prefix for error messages.
 * @param dep_count Number of dependency paths.
 * @param deps Array of dependency paths.
 * @param target Output artifact path to check against.
 * @return 0 on success, -1 on error.
 */
int frugal_mtime(
	bool* do_work,
	char const log_prefix[static 1],
	size_t dep_count,
	char* const* deps,
	char* target
);

/**
 * Check if link output needs relinking.
 *
 * Like frugal_mtime, but also handles changed flags, resolves -lXXX and -l:filename flags to actual static lib paths (searching -L dirs and the install prefix lib dir) and includes them as deps, and *also* looks at the sources like frugal_mtime would.
 * This only triggers a relink if the changed dependencies are static libraries; no need for shared objects.
 *
 * @param do_link Set to true if output needs to be relinked, false otherwise.
 * @param log_prefix Log prefix for error messages.
 * @param flags Linker flags (scanned for -L, -l, and -l: entries, as well as direct static library dependencies).
 * @param obj_count Number of object file paths.
 * @param objs Array of object file paths.
 * @param out Output artifact path to check against.
 * @return 0 on success, -1 on error.
 */
int frugal_link(
	bool* do_link,
	char const log_prefix[static 1],
	flamingo_val_t* flags,
	size_t obj_count,
	char** objs,
	char* out
);
