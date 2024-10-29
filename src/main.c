// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
# include <errno.h>
# include <sys/prctl.h>
#endif

#include <bsys.h>
#include <logging.h>

// Global options go here so they're accessible by everyone.

char const* rel_bin_path = ".bob"; // Default output path.
char* bin_path = NULL;
char const* init_name = "bob";
char const* prefix = NULL;
char const* project_path = NULL;
char* cur_instr = NULL;

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

	fprintf(
		stderr,
		"usage: %1$s [-p prefix] [-C project_directory] [-o out_directory] "
		"build\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] lsp\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] run "
		"[args ...]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] "
		"install\n"
		"       %1$s [-p prefix] [-C project_directory] skeleton skeleton_name "
		"[out_directory]\n"
		"       %1$s [-p prefix] [-C project_directory] [-o out_directory] " "package format [name] [out_file]\n",
		progname
	);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	init_name = *argv;
	logging_init();

	// Parse options.

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

	// Need to run at least one instruction.

	if (!argc) {
		usage();
	}

	// If project path wasn't set, set it to the current working directory.
	// Then, make it absolute.

	if (!project_path) {
		project_path = ".";
	}

	char const* const rel_project_path = project_path;
	project_path = realpath(rel_project_path, NULL);

	if (!project_path) {
		LOG_FATAL("Invalid project path (\"%s\")", rel_project_path);
		return EXIT_FAILURE;
	}

	// TODO Make sure relative bin path is in '.gitignore'.

	// validate_gitignore((char*) rel_bin_path);

	// Make 'init_name' absolute.
	// No biggie if we can't make it absolute, it's probably being run as a
	// standalone command, in which case 'execute_async' can find it for us later
	// by searching through 'PATH'.
	// TODO Shouldn't this be relative to 'project_path' if one is set?

	char const* const abs_init_name = realpath(init_name, NULL);

	if (abs_init_name) {
		init_name = abs_init_name;
	}

	// Identify the build system.

	bsys_t const* const bsys = bsys_identify();

	if (bsys == NULL) {
		LOG_FATAL("Could not identify build system");
		return EXIT_FAILURE;
	}

	printf("%s\n", bsys->name);

	if (bsys->setup && bsys->setup() < 0) {
		return EXIT_FAILURE;
	}

	// Parse instructions.

	int rv = EXIT_FAILURE; // I'm a pessimist.

	while (argc-- > 0) {
		cur_instr = *argv++;

		if (strcmp(cur_instr, "build") == 0) {
			// TODO rv = do_build();
		}

		else if (strcmp(cur_instr, "lsp") == 0) {
			// TODO gen_lsp_config = true;
			// TODO rv = do_build();
		}

		// Everything stops if we run the 'run' command.
		// We don't know how many arguments there'll still be.

		else if (strcmp(cur_instr, "run") == 0) {
			// TODO rv = do_run(argc, argv);
			goto done;
		}

		else if (strcmp(cur_instr, "install") == 0) {
			// TODO rv = do_install();
		}

		// Everything stops if we run the 'skeleton' command.
		// I don't wanna deal with how the output/project paths should best be
		// handled for subsequent commands.

		else if (strcmp(cur_instr, "skeleton") == 0) {
			// TODO rv = do_skeleton(argc, argv);
			goto done;
		}

		// Everything stops if we run the 'package' command.
		// The last argument is optional, so it would introduce ambiguity.

		else if (strcmp(cur_instr, "package") == 0) {
			// TODO rv = do_package(argc, argv);
			goto done;
		}

		else {
			usage();
		}

		// Stop here if there was an error in the execution of an instruction.

		if (rv != EXIT_SUCCESS) {
			goto done;
		}
	}

done:

	return rv;
}
