#define __STDC_WANT_LIB_EXT2__ 1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
	#define _GNU_SOURCE
#endif

#include <umber.h>
#define UMBER_COMPONENT "bob"

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <wren.h>

// global options go here so they're accessible by everyone

static char* bin_path = NULL;

#include "util.h"

#include "classes/cc.h"
#include "classes/file.h"
#include "classes/linker.h"

#include "instr.h"

static char const* init_name = "bob";

static void usage(void) {
#if defined(__FreeBSD__)
	char const* const progname = getprogname();
#elif defined(__Linux__)
	char progname[16];

	if (prctl(PR_GET_NAME, progname, NULL, NULL, NULL) < 0) {
		errx("prctl(PR_GET_NAME): %s", strerror(errno));
	}
#else
	char const* const progname = init_name;
#endif

	fprintf(stderr,
		"usage: %1$s [-o output path] build\n",
	progname);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	init_name = *argv;

	// parse options

	char* _bin_path = "bin"; // default output path

	int c;

	while ((c = getopt(argc, argv, "o:")) != -1) {
		if (c == 'o') {
			_bin_path = optarg;
		}

		else {
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	// need to run at least one instruction

	if (!argc) {
		usage();
	}

	// make sure output directory exists
	// create it if it doesn't

	char* cwd = getcwd(NULL, 0);

	if (!cwd) {
		errx(EXIT_FAILURE, "getcwd: %s", strerror(errno));
	}

	bin_path = strdup(_bin_path); // don't care about freeing this

	if (*bin_path == '/') { // absolute path
		while (*++bin_path == '/'); // remove prepending slashes

		if (chdir("/") < 0) {
			errx(EXIT_FAILURE, "chdir(\"/\"): %s", strerror(errno));
		}
	}

	char* bit;

	while ((bit = strsep(&bin_path, "/"))) {
		// ignore if the bit is empty

		if (!bit || !*bit) {
			continue;
		}

		// ignore if the bit refers to the current directory

		if (!strcmp(bit, ".")) {
			continue;
		}

		// don't attempt to mkdir if we're going backwards, only chdir

		if (!strcmp(bit, "..")) {
			goto no_mkdir;
		}

		if (mkdir(bit, 0700) < 0 && errno != EEXIST) {
			errx(EXIT_FAILURE, "mkdir(\"%s\"): %s", bit, strerror(errno));
		}

	no_mkdir:

		if (chdir(bit) < 0) {
			errx(EXIT_FAILURE, "chdir(\"%s\"): %s", bit, strerror(errno));
		}
	}

	// move back to current directory once we're sure the output directory exists

	if (chdir(cwd) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", cwd, strerror(errno));
	}

	// get absolute path of output directory so we don't ever get lost or confused

	bin_path = realpath(_bin_path, NULL);

	if (!bin_path) {
		errx(EXIT_FAILURE, "realpath(\"%s\"): %s", _bin_path, strerror(errno));
	}

	// parse instructions

	while (argc --> 0) {
		char const* const instr = *argv;
		int rv = EXIT_FAILURE; // I'm a pessimist

		if (!strcmp(instr, "build")) {
			rv = do_build();
		}

		else {
			usage();
		}

		// stop here if there was an error in the execution of an instruction

		if (rv != EXIT_SUCCESS) {
			return rv;
		}
	}

	// if all went well, we may rest peacefully

	return EXIT_SUCCESS;
}
