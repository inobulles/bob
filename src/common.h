// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

// this file must be included before anything else
// feature test macros go here, because they're annoying and no one likes the
// defaults see: feature_test_macros(7)

#define __STDC_WANT_LIB_EXT2__ \
	1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
# define _GNU_SOURCE
#endif

#define COMMON_INCLUDED

extern char const* out_path;

#include <stdlib.h>

__attribute__((unused)) static void strfree(char* const* str_ref) {
	char* const str = *str_ref;

	if (!str) {
		return;
	}

	free(str);
}

#define CLEANUP_STR __attribute__((cleanup(strfree)))
