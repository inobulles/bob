#include <stdbool.h>
#include <term.h>

#include "logging.h"

static bool colour_support = false;

void logging_init(void) {
	// TODO
}

__attribute__((__format__(__printf__, 3, 0)))
void vlog(FILE* stream, char const* colour, char const* const fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char* msg;
	vasprintf(&msg, fmt, args);

	va_end(args);

	fprintf(stream, "%s%s%s\n", colour, msg, CLEAR);
	free(msg);
}

