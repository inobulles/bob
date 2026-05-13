// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#pragma once

#include <stddef.h>

/**
 * Whether to generate compile_commands.json.
 *
 * Set automatically on 'bob build' if compile_commands.json already exists next to build.fl,
 * or unconditionally on 'bob lsp'.
 */
extern _Bool gen_compile_commands;

/**
 * Add an entry to the in-memory compile commands database.
 * Thread-safe.
 *
 * @param file      Source file path.
 * @param directory Absolute path to the project directory.
 * @param args      Compile command as an array of strings (not NULL-terminated).
 * @param count     Number of elements in args.
 */
void cc_compile_commands_add(char const* file, char const* directory, char* const* args, size_t count);

/**
 * Write the accumulated entries to compile_commands.json in the current directory.
 *
 * @return 0 on success, -1 on failure.
 */
int cc_compile_commands_write(void);
