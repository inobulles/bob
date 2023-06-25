#include <instr.h>

int do_run(int argc, char** argv) {
	if (navigate_project_path() < 0) {
		return EXIT_FAILURE;
	}

	if (ensure_out_path() < 0) {
		return EXIT_FAILURE;
	}

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// setup environment for running
	// TODO ideally this should happen exclusively in a child process, but I think that would be quite complicated to implement

	if (setup_env(bin_path) < 0) {
		goto err;
	}

	// call the run function

	rv = wren_call_args(&state, "Runner", "run(_)", argc, argv);

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

