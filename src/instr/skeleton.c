// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <instr.h>

#include <errno.h>
#include <unistd.h>

int do_skeleton(int argc, char** argv) {
	// parse arguments

	if (argc != 1 && argc != 2)
		usage();

	char* const name = argv[0];
	char* const out = argv[1] ? argv[1] : name;

	// make sure output directory doesn't yet exist - we wouldn't wanna overwrite anything!

	if (!access(out, F_OK)) {
		LOG_FATAL("Output directory '%s' already exists", out)
		return EXIT_FAILURE;
	}

	// make output directory

	if (mkdir_recursive(out) < 0) {
		LOG_FATAL("Failed to create output directory '%s': %s", out, strerror(errno))
		return EXIT_FAILURE;
	}

	// build skeleton path
	// XXX instead of hardcoding this format string, maybe there's a better way by passing installation directories at compile time
	// XXX also, how does this work for when bob isn't yet installed to the system?

	char* CLEANUP_STR skeleton_path = NULL;
	if (asprintf(&skeleton_path, "%s/share/bob/skeletons/%s", install_prefix(), name)) {};

	// copy over skeleton files

	char* const CLEANUP_STR err = copy_recursive(skeleton_path, out);

	if (err != NULL) {
		LOG_FATAL("Failed to copy skeleton '%s' to output directory '%s': %s", skeleton_path, out, err)
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
