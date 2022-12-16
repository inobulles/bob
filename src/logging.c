#include <sys/_stdarg.h>
#define __STDC_WANT_LIB_EXT2__ 1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
   #define _GNU_SOURCE
#endif

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"

bool colour_support = false;

static bool supports_colour(void) {
	// if we're forced to do colours, oblige 😞

	char* const clicolor_force = getenv("CLICOLOR_FORCE");

	if (clicolor_force) {
		return true;
	}

	// if stdout or stderr is not a TTY, we don't want colours

	if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)) {
		return false;
	}

	// check 'COLORTERM' (this is what ls(1) does on aquaBSD, except it also checks 'CLICOLOR' - too lazy personally)

	char* const colorterm = getenv("COLORTERM");

	if (colorterm && *colorterm) {
		return true;
	}

	// check if 'TERM' has the substring "color" in it

	char* const term = getenv("TERM");

	if (!term) {
		return false;
	}

	return strstr(term, "color");
}

void logging_init(void) {
	// does our terminal support colours?
	// this seems surprisingly stupidly difficult: https://unix.stackexchange.com/questions/573410/how-to-interact-with-a-terminfo-database-in-c-without-ncurses

	colour_support = supports_colour();
}

void vlog(FILE* stream, char const* colour, char const* const fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char* msg;
	vasprintf(&msg, fmt, args);

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

// TODO check terminal capabilities (and length if I want to eventually have a proper progress bar)

void progress_complete(progress_t* self) {
	self->frac = 1;

	printf(REPLACE_LINE);
	fflush(stdout);
}

void progress_update(progress_t* self, float frac, char const* fmt, ...) {
	self->frac = frac;

	fflush(stdout);
	printf(REPLACE_LINE BOLD BLUE "🚧 [%3d%%] " REGULAR BLUE, (int) (self->frac * 100));

	va_list args;
	va_start(args, fmt);

	vprintf(fmt, args);
	printf(CLEAR);

	va_end(args);
}
