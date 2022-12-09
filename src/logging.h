#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// prototypes

void logging_init(void);

__attribute__((__format__(__printf__, 3, 0)))
void vlog(FILE* stream, char const* colour, char const* const fmt, ...);

// kinda replicate the umber API

#define CLEAR   "\033[0m"
#define REGULAR "\033[0;"
#define BOLD    "\033[1;"

#define PURPLE  "35m"
#define RED     "31m"
#define YELLOW  "33m"
#define GREEN   "32m"

#define LOG_FATAL(...)   vlog(stderr, "💀 " BOLD    PURPLE, __VA_ARGS__);
#define LOG_ERROR(...)   vlog(stderr, "🔴 " BOLD    RED,    __VA_ARGS__);
#define LOG_WARN(...)    vlog(stderr, "⚠️  " REGULAR YELLOW, __VA_ARGS__);
#define LOG_SUCCESS(...) vlog(stderr, "🟢 " REGULAR GREEN,  __VA_ARGS__);
