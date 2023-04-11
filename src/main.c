#define __STDC_WANT_LIB_EXT2__ 1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
	#define _GNU_SOURCE
#endif

#include <string.h>
#include <unistd.h>

#include <instr.h>

// global options go here so they're accessible by everyone

char const* rel_bin_path = "bin"; // default output path
char* bin_path = NULL;
char const* init_name = "bob";
char const* curr_instr = NULL;
char const* prefix = NULL;
char const* project_path = NULL;

void usage(void) {
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
		"usage: %1$s [-p prefix] [-C project_directory] [-o out_directory] build\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] run [args ...]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] install\n"
		"       %1$s [-p prefix] [-C project_directory] skeleton skeleton_name [out_directory]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] test\n",
	progname);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	init_name = *argv;
	logging_init();

	// parse options

	int c;

	while ((c = getopt(argc, argv, "C:o:p:")) != -1) {
		if (c == 'C') {
			project_path = optarg;
		}

		else if (c == 'o') {
			rel_bin_path = optarg;
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

	// parse instructions

	while (argc --> 0) {
		curr_instr = *argv++;
		int rv = EXIT_FAILURE; // I'm a pessimist

		if (!strcmp(curr_instr, "build")) {
			rv = do_build();
		}

		else if (!strcmp(curr_instr, "run")) {
			// everything stops if we run the 'run' command, because we don't know how many arguments there'll still be
			return do_run(argc, argv);
		}

		else if (!strcmp(curr_instr, "install")) {
			rv = do_install();
		}

		else if (!strcmp(curr_instr, "skeleton")) {
			// everything stops if we run the 'skeleton' command, because I don't wanna deal how the output/project paths should best be handled for subsequent commands
			return do_skeleton(argc, argv);
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
