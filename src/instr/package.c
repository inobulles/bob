#include <instr.h>

#include <classes/package_t.h>

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

	// go to project path

	navigate_project_path();
	ensure_out_path();

	// declarations which must come before first goto

	WrenHandle* package_map_handle = NULL;
	WrenHandle* package_keys_handle = NULL;
	size_t package_keys_len = 0;

	WrenHandle* install_map_handle = NULL;
	WrenHandle* install_keys_handle = NULL;
	size_t install_keys_len = 0;

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	rv = wren_read_map(&state, PACKAGE_MAP, &package_map_handle, &package_keys_len);

	if (rv != EXIT_SUCCESS)
		goto err;

	// read package map key/value pairs

	package_keys_handle = wrenGetSlotHandle(state.vm, 0);

	wrenEnsureSlots(state.vm, 4); // first slot for the keys list, second for the key, third slot for the value, last for map
	wrenSetSlotHandle(state.vm, 3, package_map_handle);

	for (size_t i = 0; i < package_keys_len; i++) {
		// get key

		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("'%s' map element %zu key is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", PACKAGE_MAP, i)
			continue;
		}

		char const* const key = wrenGetSlotString(state.vm, 1);

		// sanity check - make sure map contains key

		if (!wrenGetMapContainsKey(state.vm, 3, 1)) {
			LOG_WARN("'%s' map does not contain key '%s'", PACKAGE_MAP, key)
			continue;
		}

		// get value

		wrenGetMapValue(state.vm, 3, 1, 2);

		if (wrenGetSlotType(state.vm, 2) != WREN_TYPE_FOREIGN) {
			LOG_WARN("'%s' map element '%s' value is of incorrect type (expected 'WREN_TYPE_FOREIGN') - skipping", PACKAGE_MAP, key)
			continue;
		}

		package_t* const package = wrenGetSlotForeign(state.vm, 2);

		printf("%s %s\n", key, package->name);
	}

	// read install map key/value pairs

	(void) install_keys_len;

err:

	if (package_keys_handle)
		wrenReleaseHandle(state.vm, package_keys_handle);

	if (package_map_handle)
		wrenReleaseHandle(state.vm, package_map_handle);

	if (install_keys_handle)
		wrenReleaseHandle(state.vm, install_keys_handle);

	if (install_map_handle)
		wrenReleaseHandle(state.vm, install_map_handle);

	wren_clean_vm(&state);

	return rv;
}
