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
char const* init_name = "bob";
char const* bootstrap_import_path = "import";

char const* out_path = ".bob"; // Default output path.
char const* abs_out_path = NULL;

char* deps_path = NULL;
bool build_deps = true;

char const* install_prefix = NULL;
char* default_final_install_prefix = NULL;
char* default_tmp_install_prefix = NULL;

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
		"usage: %1$s [-j jobs] [-p install_prefix] [-D] [-C project_directory] [-o out_directory] build\n"
		"       %1$s [-j jobs] [-p install_prefix] [-D] [-C project_directory] [-o out_directory] run [args ...]\n"
		"       %1$s [-j jobs] [-p install_prefix] [-D] [-C project_directory] [-o out_directory] sh [args ...]\n"
		"       %1$s [-j jobs] [-p install_prefix] [-D] [-C project_directory] [-o out_directory] " "install\n",
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

	while ((c = getopt(argc, argv, "C:Dj:o:p:")) != -1) {
		switch (c) {
		case 'C':
			project_path = optarg;
			break;
		case 'D':
			build_deps = false;
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

	// Make 'init_name' absolute if it's detected to be a path (i.e. contains a slash).
	// No biggie if we can't make it absolute, it's probably being run as a
	// standalone command, in which case 'execute_async' can find it for us later
	// by searching through 'PATH'.
	// We must do this before potentially chdir'ing because this shouldn't be relative to 'project_path'.
	// The reason we only want to do this when 'init_name' is a path is because we could have a file or directory named 'init_name' in the project directory, even when it was actually run as a command.

	if (strstr(init_name, "/") != NULL) {
		char const* const abs_init_name = realerpath(init_name);

		if (abs_init_name != NULL) {
			init_name = abs_init_name;
		}
	}

	// Get install prefix and ensure it exists.
	// We must do this before chdir'ing - don't think I need to explain.

	if (install_prefix == NULL) {
		install_prefix = getenv("PREFIX");
	}

	if (install_prefix != NULL) {
		install_prefix = realerpath(install_prefix);

		if (install_prefix == NULL) {
			LOG_FATAL("Install prefix path \"%s\" doesn't exist.", install_prefix);
			return EXIT_FAILURE;
		}
	}

	// If project path wasn't set, set it to the current working directory.
	// Then, make it absolute.
	// Finally, actually change to that directory.

	if (!project_path) {
		project_path = ".";
	}

	char const* const rel_project_path = project_path;
	project_path = realerpath(rel_project_path);

	if (project_path == NULL) {
		LOG_FATAL("Project path \"%s\" doesn't exist.", rel_project_path);
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

	// Get default final and temporary install prefixes.

	default_final_install_prefix = "/usr/local";
	asprintf(&default_tmp_install_prefix, "%s/prefix", abs_out_path);
	assert(default_tmp_install_prefix != NULL);

	// Ensure all installation prefixes (explicitly set, final, and temporary) exist.

	char const* const prefixes[] = {
		install_prefix,
		default_final_install_prefix,
		default_tmp_install_prefix
	};

	for (size_t i = 0; i < sizeof prefixes / sizeof *prefixes; i++) {
		char const* const prefix = prefixes[i];

		if (prefix == NULL) {
			continue;
		}

		if (mkdir_wrapped(prefix, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", prefix, strerror(errno));
			return -1;
		}
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
		if (bsys_build(bsys) == 0) {
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

	// This is intentionally undocumented, as it's really only used for communication between Bob parent processes and their Bob children processes.

	else if (strcmp(instr, "dep-tree") == 0) {
		LOG_WARN("This command is internal and isn't meant for direct use. A correct consumer of this command should be able to discard this message by reading the contents within the dependency tree tags.");

		if (bsys_dep_tree(bsys, argc, argv) == 0) {
			rv = EXIT_SUCCESS;
		}
	}

	else {
		LOG_FATAL("Unknown instruction '%s'.", instr);
		usage();
	}

	free_build_steps();

	return rv;
}
