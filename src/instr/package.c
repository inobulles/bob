#include <instr.h>

// for ZPK packages:
// - read installation map
// - copy over relevant files
// - set entry node to what was passed to constructor
// - add metadata files
// - package (libiar?)

// for FreeBSD packages (and this will be the same idea for package formats I'd like to support in the future):
// - read installation map
// - copy over relevant files
// - translate installation map to manifest
// - add metadata to manifest
// - package (libpkg? or is it gun be easier to just 'pkg-create(8)'?)

static bool is_supported(char* format) {
	bool supported = false;

	supported |= !strcmp(format, "zpk");
	// supported |= !strcmp(format, "fbsd");

	return supported;
}

int do_package(int argc, char** argv) {
	// parse arguments

	if (argc < 1 || argc > 3)
		usage();

	char* const format = argv[0];

	if (!is_supported(format))
		usage();

	char* const name = argc >= 2 ? argv[1] : "package";

	char* __attribute__((cleanup(strfree))) out = NULL;

	if (argc == 3)
		out = strdup(argv[2]);

	else if (asprintf(&out, "%s.%s", name, format)) {}

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

err:

	wren_clean_vm(&state);

	return rv;
}
