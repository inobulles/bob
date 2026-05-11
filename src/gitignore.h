// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#pragma once

/**
 * Check that the output path is listed in .gitignore.
 *
 * Warns if .gitignore doesn't exist or if out_path is not found in it.
 *
 * @param out_path Output path to look for (e.g. ".bob").
 */
void check_gitignore(char const* out_path);
