#include <instr.h>

#include <string.h>

int do_install(void) {
	navigate_project_path();
	ensure_out_path();

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	rv = install(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

err:

	wren_clean_vm(&state);

	return rv;
}
