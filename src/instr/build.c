#include <instr.h>

int do_build(void) {
	if (navigate_project_path() < 0) {
		return EXIT_FAILURE;
	}

	if (ensure_out_path() < 0) {
		return EXIT_FAILURE;
	}

	state_t state = { 0 };
	int const rv = wren_setup_vm(&state);

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
