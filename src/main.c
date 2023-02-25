#include "util.h"
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

#include <sys/stat.h>
#include <sys/wait.h>

#include <wren.h>

// global options go here so they're accessible by everyone

char* bin_path = NULL;
char const* init_name = "bob";
char const* curr_instr = NULL;
char const* prefix = NULL;

#include "instr.h"

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
		"usage: %1$s [-p prefix] [-C project directory] [-o out directory] build\n"
		"       %1$s [-p prefix] [-C project directory] [-o out directory] run\n"
		"       %1$s [-p prefix] [-C project directory] [-o out directory] install\n"
		"       %1$s [-p prefix] [-C project directory] [-o out directory] test\n",
	progname);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	init_name = *argv;
	logging_init();

	// parse options

	char* _bin_path = "bin"; // default output path
	char* project_path = NULL;

	int c;

	while ((c = getopt(argc, argv, "C:o:p:")) != -1) {
		if (c == 'C') {
			project_path = optarg;
		}

		else if (c == 'o') {
			_bin_path = optarg;
		}

		else if (c == 'p') {
			prefix = optarg;
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

	// make 'init_name' absolute
	// no biggie if we can't make it absolute, it's probably being run as a standalone command, in which case 'execute_async' can find it for us later by searching through 'PATH'

	char const* const abs_init_name = realpath(init_name, NULL);

	if (abs_init_name) {
		init_name = abs_init_name;
	}

	// navigate into project directory, if one was specified

	if (project_path && chdir(project_path) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", project_path, strerror(errno));
	}

	// make sure output directory exists
	// create it if it doesn't

	if (mkdir_recursive(_bin_path) < 0) {
		errx(EXIT_FAILURE, "mkdir_recursive(\"%s\"): %s", _bin_path, strerror(errno));
	}

	// get absolute path of output directory so we don't ever get lost or confused

	bin_path = realpath(_bin_path, NULL);

	if (!bin_path) {
		errx(EXIT_FAILURE, "realpath(\"%s\"): %s", _bin_path, strerror(errno));
	}

	// parse instructions

	while (argc --> 0) {
		curr_instr = *argv++;
		int rv = EXIT_FAILURE; // I'm a pessimist

		if (!strcmp(curr_instr, "build")) {
			rv = do_build();
		}

		else if (!strcmp(curr_instr, "run")) {
			// everything stops if we run the 'run' command
			return do_run(argc, argv);
		}

		else if (!strcmp(curr_instr, "install")) {
			rv = do_install();
		}

		else if (!strcmp(curr_instr, "test")) {
			rv = do_test();
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
