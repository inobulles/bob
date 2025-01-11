// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

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

extern _Bool debugging;
extern char const* instr;
extern char const* init_name;
extern char const* bootstrap_import_path;

extern char const* out_path;
extern char const* abs_out_path;
extern char* bsys_out_path;

extern char* deps_path;
extern _Bool build_deps;

extern _Bool own_prefix;
extern char const* install_prefix;
extern char* default_final_install_prefix;
extern char* default_tmp_install_prefix;
