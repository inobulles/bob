// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// external variables

extern bool colour_support;

// structs

typedef struct {
	float frac;
} progress_t;

// prototypes

void logging_init(void);

__attribute__((__format__(__printf__, 3, 0))) void
vlog(FILE* stream, char const* colour, char const* const fmt, ...);

// progress bar stuff

progress_t* progress_new(void);
void progress_del(progress_t* self);

void progress_complete(progress_t* self);

__attribute__((__format__(__printf__, 4, 0))) void
progress_update(progress_t* self, size_t numerator, size_t denominator, char const* fmt, ...);

// Other logging utilities.

void log_already_done(char const* cookie, char const* prefix, char const* past);

// kinda replicate the umber API

#define CLEAR "\033[0m"
#define REGULAR "\033[0;"
#define BOLD "\033[1;"

#define PURPLE "35m"
#define RED "31m"
#define YELLOW "33m"
#define GREEN "32m"
#define BLUE "34m"

#define REPLACE_LINE "\33[2K\r"

#define LOG_FATAL(...) vlog(stderr, "💀 " BOLD PURPLE, __VA_ARGS__)
#define LOG_ERROR(...) vlog(stderr, "🛑 " BOLD RED, __VA_ARGS__)
#define LOG_WARN(...) vlog(stderr, "🚸 " REGULAR YELLOW, __VA_ARGS__)
#define LOG_INFO(...) vlog(stdout, "👷 " REGULAR BLUE, __VA_ARGS__)
#define LOG_SUCCESS(...) vlog(stdout, "✅ " REGULAR GREEN, __VA_ARGS__)

#define PLZ_REPORT " This is a serious internal issue, please report it!"
