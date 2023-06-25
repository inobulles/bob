#include <instr.h>

#include <string.h>
#include <unistd.h>

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
	char progname[16] = init_name;

	if (prctl(PR_GET_NAME, progname, NULL, NULL, NULL) < 0) {
		LOG_WARN("prctl(PR_GET_NAME): %s", strerror(errno))
	}
#else
	char const* const progname = init_name;
#endif

	fprintf(stderr,
		"usage: %1$s [-p prefix] [-C project_directory] [-o out_directory] build\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] run [args ...]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] install\n"
		"       %1$s [-p prefix] [-C project_directory] skeleton skeleton_name [out_directory]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] package format [name] [out_file]\n"
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

	int rv = EXIT_FAILURE; // I'm a pessimist

	while (argc --> 0) {
		curr_instr = *argv++;

		if (!strcmp(curr_instr, "build")) {
			rv = do_build();
		}

		// everything stops if we run the 'run' command
		// we don't know how many arguments there'll still be

		else if (!strcmp(curr_instr, "run")) {
			rv = do_run(argc, argv);
			goto done;
		}

		else if (!strcmp(curr_instr, "install")) {
			rv = do_install();
		}

		// everything stops if we run the 'skeleton' command
		// I don't wanna deal with how the output/project paths should best be handled for subsequent commands

		else if (!strcmp(curr_instr, "skeleton")) {
			rv = do_skeleton(argc, argv);
			goto done;
		}

		// everything stops if we run the 'package' command
		// the last argument is optional, so it would introduce ambiguity

		else if (!strcmp(curr_instr, "package")) {
			rv = do_package(argc, argv);
			goto done;
		}

		else if (!strcmp(curr_instr, "test")) {
			rv = do_test();
		}

		else {
			usage();
		}

		// stop here if there was an error in the execution of an instruction

		if (rv != EXIT_SUCCESS) {
			goto done;
		}
	}

done:

	return rv;
}
