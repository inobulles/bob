#include <instr.h>

#include <string.h>

int do_install(void) {
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

	// actually install

	rv = install(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

err:

	wren_clean_vm(&state);

	if (fix_out_path_owner() < 0) {
		return EXIT_FAILURE;
	}

	return rv;
}
