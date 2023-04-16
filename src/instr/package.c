#include <instr.h>

#include <classes/package_t.h>

#include <errno.h>

#include <sys/stat.h>

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

typedef enum {
	FORMAT_UNSUPPORTED,
	FORMAT_ZPK,
	FORMAT_FBSD,
} format_t;

typedef int (*stage_populate_fn_t) (char* path);

static int stage_populate_unsupported(char* path) {
	LOG_FATAL("Unsupported packaging format (staging path is \"%s\")", path)
	return -1;
}

static int stage_populate_zpk(char* path) {
	(void) path;
	return 0;
}

static int stage_populate_fbsd(char* path) {
	LOG_FATAL("FreeBSD packages are not yet supported (staging path is \"%s\")", path)
	return -1;
}

static stage_populate_fn_t stage_populate_fns[] = {
	stage_populate_unsupported,
	stage_populate_zpk,
	stage_populate_fbsd,
};

static format_t get_format(char* format) {
	if (!strcmp(format, "zpk"))
		return FORMAT_ZPK;

	// if (!strcmp(format, "fbsd"))
	// 	return FORMAT_FBSD;

	return FORMAT_UNSUPPORTED;
}

int do_package(int argc, char** argv) {
	// parse arguments

	if (argc < 1 || argc > 3)
		usage();

	char* const format_str = argv[0];
	format_t const format = get_format(format_str);

	if (format == FORMAT_UNSUPPORTED)
		usage();

	char* const name = argc >= 2 ? argv[1] : "default";

	char* __attribute__((cleanup(strfree))) out = NULL;

	if (argc == 3)
		out = strdup(argv[2]);

	else if (asprintf(&out, "%s.%s", name, format_str)) {}

	// go to project path

	navigate_project_path();
	ensure_out_path();

	// declarations which must come before first goto

	WrenHandle* package_map_handle = NULL;
	WrenHandle* package_keys_handle = NULL;
	size_t package_keys_len = 0;

	char* __attribute__((cleanup(strfree))) staging_path = NULL;

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
	// look for the package name asked of us

	char const* key = NULL;

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

		key = wrenGetSlotString(state.vm, 1);

		// sanity check - make sure map contains key

		if (!wrenGetMapContainsKey(state.vm, 3, 1)) {
			LOG_WARN("'%s' map does not contain key '%s'", PACKAGE_MAP, key)
			continue;
		}

		// is the key what we're looking for?

		if (!strcmp(key, name))
			goto found;
	}

	// found nothing :'(

	LOG_FATAL("'%s' map doesn't contain package '%s'", PACKAGE_MAP, name)

found:

	// get value

	wrenGetMapValue(state.vm, 3, 1, 2);

	if (wrenGetSlotType(state.vm, 2) != WREN_TYPE_FOREIGN) {
		LOG_FATAL("'%s' map element '%s' value is of incorrect type (expected 'WREN_TYPE_FOREIGN')", PACKAGE_MAP, key)
		goto err;
	}

	package_t* const package = wrenGetSlotForeign(state.vm, 2);

	printf("Found package '%s'\n", package->name);

	// create package staging directory

	if (asprintf(&staging_path, "%s/%s", bin_path, out)) {}

	if (mkdir(staging_path, 0660) < 0 && errno != EEXIST) {
		LOG_FATAL("Can't create package staging path: mkdir(\"%s\"): %s", staging_path, strerror(errno))
		goto err;
	}

	// format specific population

	size_t const fn_count = sizeof(stage_populate_fns) / sizeof(*stage_populate_fns);

	if ((size_t) format > fn_count) {
		LOG_FATAL("Sanity check failed: there are %zu stage population functions, but format is %d", fn_count, format)
		goto err;
	}

	if (stage_populate_fns[format](staging_path) < 0)
		goto err;

	// TODO read install map key/value pairs

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
