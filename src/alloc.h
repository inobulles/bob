// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

/*
 * Checked versions of memory-allocating functions.
 *
 * These just assert that the allocated pointer is non-NULL.
 */

#pragma once

#include <common.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void* malloc_c(size_t size) {
	void* ptr = malloc(size);
	assert(ptr != NULL);
	return ptr;
}

static inline void* calloc_c(size_t n, size_t size) {
	void* ptr = calloc(n, size);
	assert(ptr != NULL);
	return ptr;
}

static inline void* realloc_c(void* ptr, size_t size) {
	void* new_ptr = realloc(ptr, size);
	assert(new_ptr != NULL);
	return new_ptr;
}

static inline char* strdup_c(char const* s) {
	char* dup = strdup(s);
	assert(dup != NULL);
	return dup;
}

static inline char* strndup_c(char const* s, size_t n) {
	char* dup = strndup(s, n);
	assert(dup != NULL);
	return dup;
}

static inline void vasprintf_c(char** strp, char const* fmt, va_list va) {
	vasprintf(strp, fmt, va);
	assert(*strp != NULL);
}

static inline __attribute__((format(printf, 2, 3))) void asprintf_c(char** strp, char const* fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vasprintf_c(strp, fmt, va);
	va_end(va);
}
