// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#define __STDC_WANT_LIB_EXT2__ \
	1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
# define _GNU_SOURCE
#endif

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <logging.h>
#include <str.h>

bool colour_support = false;

static bool supports_colour(void) {
	// if we're forced to do colours, oblige 😞

	bool const clicolor_force = !!getenv("CLICOLOR_FORCE");

	if (clicolor_force) {
		return true;
	}

	// if stdout or stderr is not a TTY, we don't want colours

	if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)) {
		return false;
	}

	// check 'COLORTERM' (this is what ls(1) does on aquaBSD, except it also
	// checks 'CLICOLOR' - too lazy personally)

	char* const colorterm = getenv("COLORTERM");

	if (colorterm && *colorterm) {
		return true;
	}

	// check if 'TERM' has the substring "color" in it

	char* const term = getenv("TERM");

	if (!term) {
		return false;
	}

	return !!strstr(term, "color");
}

void logging_init(void) {
	// does our terminal support colours?
	// this seems surprisingly stupidly difficult:
	// https://unix.stackexchange.com/questions/573410/how-to-interact-with-a-terminfo-database-in-c-without-ncurses

	colour_support = supports_colour();
}

void vlog(FILE* stream, char const* colour, char const* const fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char* msg = NULL;
	vasprintf(&msg, fmt, args);
	assert(msg != NULL);

	va_end(args);

	if (colour_support) {
		fprintf(stream, "%s%s%s\n", colour, msg, CLEAR);
	}

	else {
		fprintf(stream, "%s\n", msg);
	}

	free(msg);
}

progress_t* progress_new(void) {
	progress_t* self = calloc(1, sizeof *self);
	self->frac = 0;

	return self;
}

void progress_del(progress_t* self) {
	free(self);
}

void progress_complete(progress_t* self) {
	self->frac = 1;

	printf(REPLACE_LINE);
	fflush(stdout);
}

#define BAR_SIZE 16

void progress_update(progress_t* self, size_t numerator, size_t _denominator, char const* fmt, ...) {
	float const denominator = (float) _denominator - 1;
	self->frac = 0.5; // if something goes wrong (e.g. there's only one item in
							// our progress bar), default to 50%

	if (denominator > 0) {
		self->frac = numerator / denominator;
	}

	printf(colour_support ? REPLACE_LINE BOLD BLUE "🚧 [" : "🚧 [");

	for (size_t i = 0; i < BAR_SIZE; i++) {
		printf(i < self->frac * (BAR_SIZE + 1) ? "=" : ".");
	}

	printf(colour_support ? " %3d%%] " REGULAR BLUE : " %3d%%] ", (int) (self->frac * 100));

	va_list args;
	va_start(args, fmt);

	vprintf(fmt, args);
	printf(CLEAR);
	fflush(stdout);

	va_end(args);
}

void log_already_done(char const* cookie, char const* prefix, char const* past) {
	char* CLEANUP_STR path;
	asprintf(&path, "%s.log", cookie);
	assert(path != NULL);

	char* CLEANUP_STR out = NULL;
	FILE* const f = fopen(path, "r");

	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		size_t const size = ftell(f);

		out = malloc(size + 1);
		assert(out != NULL);

		rewind(f);
		fread(out, 1, size, f);
		out[size] = '\0';

		fclose(f);
	}

	char* const suffix = out ? ":" : ".";

	LOG_SUCCESS("%s" CLEAR "%sAlready %s%s", prefix ? prefix : "", prefix ? ": " : "", past, suffix);

	if (out) {
		printf("%s", out);
	}
}
