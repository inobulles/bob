// SPDX-License-Identifier: MIT
// Copyright (c) 2023-2025 Aymeric Wibo

#pragma once

// This file must be included before anything else.
// Feature test macros go here, because they're annoying and no one likes the defaults.
// See: feature_test_macros(7).

#define __STDC_WANT_LIB_EXT2__ \
	1 // ISO/IEC TR 24731-2:2010 standard library extensions.

#if __linux__
# define _GNU_SOURCE
#endif

#define COMMON_INCLUDED

#define VERSION "v0.2.19"

/**
 * Whether the BOB_BUIlD_DEBUGGING envvar is set.
 */
extern _Bool debugging;

/**
 * The instruction (i.e. subcommand) being run (like build, install, &c).
 */
extern char const* instr;

/**
 * The initial name of the binary.
 *
 * This is used if this process needs to spawn any child Bob processes.
 */
extern char const* init_name;

/**
 * The Flamingo bootstrap import path for Bob.
 *
 * This is set to "import/" in the current directory (before changing with -C), so that Bob can be run bootstrapped before it is actually installed.
 */
extern char const* bootstrap_import_path;

/**
 * The absolute ".bob" output path.
 */
extern char const* abs_out_path;

/**
 * The current build system's output path, including the ".bob" prefix.
 *
 * E.g. ".bob/bob/", or ".bob/meson/".
 */
extern char* bsys_out_path;

/**
 * The dependency cache path.
 *
 * Set to the BOB_DEPS_PATH envvar if set.
 */
extern char* deps_path;

/**
 * Whether or not to build dependencies.
 *
 * This is for when the current process is supposed to just be building itself, because the parent is handling spawning processes for building the entire dependency tree we are part of.
 */
extern _Bool build_deps;

/**
 * TODO Document this.
 */
extern _Bool own_prefix;

/**
 * Default final installation prefix.
 *
 * Set to '/usr/local/'.
 */
extern char* default_final_install_prefix;

/**
 * Default temporary installation prefix.
 *
 * Set to '.bob/prefix/' (depending on the value of {@link abs_out_path}).
 */
extern char* default_tmp_install_prefix;

/**
 * Prefix where everything will be installed when running "bob install".
 *
 * This can be explicitly set with the '-p' switch.
 * Otherwise, it will be set to one of {@link default_final_install_prefix} or {@link default_tmp_install_prefix} depending on if we use the "build" or "install" instruction.
 */
extern char const* install_prefix;

/**
 * Forces dependency tree to be rebuilt when set.
 *
 * Used to propagate dependency tree rebuild through all children processes.
 */
extern _Bool force_dep_tree_rebuild;
