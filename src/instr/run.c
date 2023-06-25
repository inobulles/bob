#include <instr.h>

int do_run(int argc, char** argv) {
	navigate_project_path();
	ensure_out_path();

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	// setup environment for running
	// TODO ideally this should happen exclusively in a child process, but I think that would be quite complicated to implement

	setup_env(bin_path);

	// call the run function

	rv = wren_call_args(&state, "Runner", "run(_)", argc, argv);

	if (rv != EXIT_SUCCESS)
		goto err;

err:

	wren_clean_vm(&state);
	fix_out_path_owner();

	return rv;
}

