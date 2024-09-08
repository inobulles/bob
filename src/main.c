// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <instr.h>

#include <string.h>
#include <unistd.h>

#if defined(__linux__)
# include <sys/prctl.h>
# include <errno.h>
#endif

// global options go here so they're accessible by everyone

char const* rel_bin_path = "bin"; // default output path
char* bin_path = NULL;
char const* init_name = "bob";
char const* curr_instr = NULL;
char const* prefix = NULL;
char const* project_path = NULL;
bool gen_lsp_config = false;
bool ran_as_dep = false;

void usage(void) {
#if defined(__FreeBSD__)
	char const* const progname = getprogname();
#elif defined(__linux__)
	char progname[16];
	strncpy(progname, init_name, sizeof progname);

	if (prctl(PR_GET_NAME, progname, NULL, NULL, NULL) < 0) {
		LOG_WARN("prctl(PR_GET_NAME): %s", strerror(errno));
	}
#else
	char const* const progname = init_name;
#endif

	fprintf(stderr,
		"usage: %1$s [-p prefix] [-C project_directory] [-o out_directory] build\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] lsp\n"
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

	while ((c = getopt(argc, argv, "C:do:p:")) != -1) {
		if (c == 'C') {
			project_path = optarg;
		}

		else if (c == 'd') { // undocumented, as for internal use only!
			ran_as_dep = true;
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

	// if project path wasn't set, set it to the current working directory
	// then, make it absolute

	if (!project_path) {
		project_path = ".";
	}

	char const* const rel_project_path = project_path;
	project_path = realpath(rel_project_path, NULL);

	if (!project_path) {
		LOG_FATAL("Invalid project path (\"%s\")", rel_project_path);
		return EXIT_FAILURE;
	}

	// make sure relative bin path is in .gitignore

	validate_gitignore((char*) rel_bin_path);

	// make 'init_name' absolute
	// no biggie if we can't make it absolute, it's probably being run as a standalone command, in which case 'execute_async' can find it for us later by searching through 'PATH'
	// TODO shouldn't this be relative to 'project_path' if one is set?

	char const* const abs_init_name = realpath(init_name, NULL);

	if (abs_init_name) {
		init_name = abs_init_name;
	}

	// parse instructions

	int rv = EXIT_FAILURE; // I'm a pessimist

	while (argc --> 0) {
		curr_instr = *argv++;

		if (strcmp(curr_instr, "build") == 0) {
			rv = do_build();
		}

		else if (strcmp(curr_instr, "lsp") == 0) {
			gen_lsp_config = true;
			rv = do_build();
		}

		// everything stops if we run the 'run' command
		// we don't know how many arguments there'll still be

		else if (strcmp(curr_instr, "run") == 0) {
			rv = do_run(argc, argv);
			goto done;
		}

		else if (strcmp(curr_instr, "install") == 0) {
			rv = do_install();
		}

		// everything stops if we run the 'skeleton' command
		// I don't wanna deal with how the output/project paths should best be handled for subsequent commands

		else if (strcmp(curr_instr, "skeleton") == 0) {
			rv = do_skeleton(argc, argv);
			goto done;
		}

		// everything stops if we run the 'package' command
		// the last argument is optional, so it would introduce ambiguity

		else if (strcmp(curr_instr, "package") == 0) {
			rv = do_package(argc, argv);
			goto done;
		}

		else if (strcmp(curr_instr, "test") == 0) {
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
