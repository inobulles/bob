// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <build_step.h>
#include <fsutil.h>
#include <logging.h>
#include <ncpu.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
# include <errno.h>
# include <sys/prctl.h>
#endif

// Global options go here so they're accessible by everyone.

bool debugging = false;
char const* out_path = ".bob"; // Default output path.
char const* abs_out_path = NULL;
char const* install_prefix = NULL;
char* deps_path = NULL;
char const* init_name = "bob";
char const* bootstrap_import_path = "import";

bool running_as_root = false;
uid_t owner = 0;

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
		"usage: %1$s [-j jobs] [-p install_prefix] [-C project_directory] [-o out_directory] build\n"
		"       %1$s [-j jobs] [-p install_prefix] [-C project_directory] [-o out_directory] run [args ...]\n"
		"       %1$s [-j jobs] [-p install_prefix] [-C project_directory] [-o out_directory] sh [args ...]\n"
		"       %1$s [-j jobs] [-p install_prefix] [-C project_directory] [-o out_directory] " "install\n",
		progname
	);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	init_name = argv[0];
	char const* project_path = NULL;
	logging_init();

	// Parse options.

	int c;

	while ((c = getopt(argc, argv, "C:j:o:p:")) != -1) {
		switch (c) {
		case 'C':
			project_path = optarg;
			break;
		case 'j':
			if (set_max_jobs(atoi(optarg)) < 0) {
				exit(EXIT_FAILURE);
			}

			break;
		case 'o':
			out_path = optarg;
			break;
		case 'p':
			install_prefix = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	// Need to run at least one instruction.

	if (!argc) {
		usage();
	}

	// Are we in build debugging mode?

	debugging = getenv("BOB_BUILD_DEBUGGING") != NULL;

	if (debugging) {
		LOG_INFO("Build debugging mode enabled.");

		if (set_max_jobs(1) < 0) {
			exit(EXIT_FAILURE);
		}
	}

	// Get the absolute bootstrap import path.
	// We do this now as we might chdir into the project directory later.
	// It's fine if realerpath returns NULL; this just means we're not bootstrapping.

	bootstrap_import_path = realerpath(bootstrap_import_path);

	// If project path wasn't set, set it to the current working directory.
	// Then, make it absolute.
	// Finally, actually change to that directory.

	if (!project_path) {
		project_path = ".";
	}

	char const* const rel_project_path = project_path;
	project_path = realerpath(rel_project_path);

	if (project_path == NULL) {
		LOG_FATAL("Invalid project path (\"%s\")", rel_project_path);
		return EXIT_FAILURE;
	}

	if (chdir(project_path) < 0) {
		LOG_FATAL("chdir(\"%s\"): %s", project_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// Get the owner of the project directory.
	// If running as root, we'll use this owner for the binary output directory.

	if (getuid() == 0) {
		struct stat sb;

		if (stat(".", &sb) < 0) {
			LOG_FATAL("stat(\"%s\"): %s", project_path, strerror(errno));
			return EXIT_FAILURE;
		}

		running_as_root = true;
		owner = sb.st_uid;
	}

	// Ensure the output path exists.

	if (mkdir_wrapped(out_path, 0755) < 0 && errno != EEXIST) {
		LOG_FATAL("mkdir(\"%s\"): %s", out_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// Get absolute output path.

	abs_out_path = realerpath(out_path);

	if (abs_out_path == NULL) {
		LOG_FATAL("realpath(\"%s\"): %s", out_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// Get system install prefix.

	if (install_prefix == NULL) {
		install_prefix = getenv("PREFIX");
	}

	if (install_prefix == NULL) {
		install_prefix = "/usr/local";
	}

	// Ensure it exists.

	if (access(install_prefix, F_OK) < 0) {
		LOG_FATAL("Installation prefix \"%s\" does not exist.", install_prefix);
		return EXIT_FAILURE;
	}

	// Get the dependencies path.

	deps_path = getenv("BOB_DEPS_PATH");

	if (deps_path == NULL) {
		char const* const home = getenv("HOME");

		// XXX Don't worry about freeing these.

		if (home != NULL) {
			asprintf(&deps_path, "%s/%s", home, ".cache/bob/deps");
		}

		else {
			asprintf(&deps_path, "%s/%s", abs_out_path, "deps");
			LOG_WARN("$HOME is not set, using '%s' as the dependencies path as a last resort.", deps_path);
		}

		assert(deps_path != NULL);
	}

	// Ensure it exists.

	if (mkdir_recursive(deps_path, 0755) < 0) {
		LOG_FATAL("mkdir_recursive(\"%s\"): %s", deps_path, strerror(errno));
		return EXIT_FAILURE;
	}

	// TODO Make sure relative bin path is in '.gitignore'.

	// validate_gitignore((char*) rel_bin_path);

	// Make 'init_name' absolute.
	// No biggie if we can't make it absolute, it's probably being run as a
	// standalone command, in which case 'execute_async' can find it for us later
	// by searching through 'PATH'.
	// TODO Shouldn't this be relative to 'project_path' if one is set?

	char const* const abs_init_name = realerpath(init_name);

	if (abs_init_name != NULL) {
		init_name = abs_init_name;
	}

	// Identify the build system.

	bsys_t const* const bsys = bsys_identify();

	if (bsys == NULL) {
		LOG_FATAL("Could not identify build system.");
		return EXIT_FAILURE;
	}

	if (bsys->setup && bsys->setup() < 0) {
		return EXIT_FAILURE;
	}

	// Parse instructions.

	int rv = EXIT_FAILURE;

	if (argc-- == 0) {
		LOG_FATAL("No instructions given.");
		usage();
	}

	char const* const instr = *argv++;

	if (strcmp(instr, "build") == 0) {
		if (bsys_build(bsys, NULL) == 0) {
			rv = EXIT_SUCCESS;
		}
	}

	else if (strcmp(instr, "run") == 0) {
		if (bsys_run(bsys, argc, argv) == 0) {
			rv = EXIT_SUCCESS;
		}
	}

	else if (strcmp(instr, "sh") == 0) {
		if (bsys_sh(bsys, argc, argv) == 0) {
			rv = EXIT_SUCCESS;
		}
	}

	else if (strcmp(instr, "install") == 0) {
		if (bsys_install(bsys) == 0) {
			rv = EXIT_SUCCESS;
		}
	}

	// This is purposefully undocumented, as it's really only used for communication between Bob parent processes and their Bob children processes.

	else if (strcmp(instr, "dep-tree") == 0) {
	}

	else {
		LOG_FATAL("Unknown instruction '%s'.", instr);
		usage();
	}

	free_build_steps();

	return rv;
}
