// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <instr.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

int do_package(int argc, char** argv) {
	// parse arguments

	if (argc < 1 || argc > 3) {
		usage();
	}

	char* const format_str = argv[0];
	char* const name = argc >= 2 ? argv[1] : "default";
	char* out = argc == 3 ? argv[2] : NULL;

	// go to project path

	if (navigate_project_path() < 0) {
		return EXIT_FAILURE;
	}

	if (ensure_out_path() < 0) {
		return EXIT_FAILURE;
	}

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// actually package

	rv = package(&state, format_str, name, out);
	wren_clean_vm(&state);

err:

	if (fix_out_path_owner() < 0) {
		return EXIT_FAILURE;
	}

	return rv;
}
