// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#pragma once

/**
 * Check that relevant generated paths are listed in .gitignore.
 *
 * Warns if .gitignore doesn't exist or if the output path / compile_commands.json are not in it.
 * compile_commands.json is only checked if the file currently exists.
 */
void check_gitignore(void);
