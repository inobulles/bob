#include <instr.h>

int do_build(void) {
	navigate_project_path();
	ensure_out_path();

	state_t state = { 0 };
	int const rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

err:

	wren_clean_vm(&state);

	return rv;
}
