#include <instr.h>

#include <classes/package_t.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

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

typedef int (*stage_populate_fn_t) (package_t* package);
typedef int (*stage_package_fn_t) (package_t* package, char* stage, char* out);

typedef struct {
	stage_populate_fn_t stage_populate;
	stage_package_fn_t stage_package;
} format_info_t;

// stage population functions per package format

static int stage_populate_unsupported(package_t* package) {
	LOG_FATAL("Unsupported packaging format (package '%s')", package->name)
	return -1;
}

static int stage_populate_zpk(package_t* package) {
	// properties necessary for the app to function

	path_write_str("unique", package->unique);
	path_write_str("start", "native");
	path_write_str("entry", package->entry);

	// metadata properties

	path_write_str("name", package->name);
	path_write_str("description", package->description);
	path_write_str("version", package->version);

	path_write_str("author", package->author);
	path_write_str("organization", package->organization);

	// truly optional metadata properties

	path_write_str("www", package->www);

	return 0;
}

static int stage_populate_fbsd(package_t* package) {
	LOG_FATAL("FreeBSD packages are not yet supported (package '%s')", package->name)
	return -1;
}

// stage packaging functions per package format

static int stage_package_unsupported(package_t* package, char* stage, char* out) {
	(void) out;
	(void) stage;

	LOG_FATAL("Unsupported packaging format (package '%s')", package->name)
	return -1;
}

static int stage_package_zpk(package_t* package, char* stage, char* out) {
	(void) package;

	int rv = 0;

	// much easier than using libiar, because it means it isn't a build dependency

	exec_args_t* const exec_args = exec_args_new(5, "iar", "--pack", stage, "--output", out);

	if (execute(exec_args) == EXIT_FAILURE)
		rv = -1;

	exec_args_del(exec_args);

	return rv;
}

static int stage_package_fbsd(package_t* package, char* stage, char* out) {
	(void) out;
	(void) stage;

	LOG_FATAL("FreeBSD packages are not yet supported (package '%s')", package->name)
	return -1;
}

static format_info_t const FORMAT_LUT[] = {
	{
		.stage_populate = stage_populate_unsupported,
		.stage_package = stage_package_unsupported,
	},
	{
		.stage_populate = stage_populate_zpk,
		.stage_package = stage_package_zpk,
	},
	{
		.stage_populate = stage_populate_fbsd,
		.stage_package = stage_package_fbsd,
	},
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

	char* CLEANUP_STR out = NULL;

	if (argc == 3)
		out = strdup(argv[2]);

	else if (asprintf(&out, "%s.%s", name, format_str)) {}

	// go to project path

	navigate_project_path();
	ensure_out_path();

	// set prefix to staging path

	char* CLEANUP_STR link_name = NULL;
	if (asprintf(&link_name, "%s/%s", bin_path, out)) {}

	char* CLEANUP_STR staging_path = NULL;
	if (asprintf(&staging_path, "%s.stage", link_name)) {}

	prefix = staging_path;

	// declarations which must come before first goto

	WrenHandle* package_map_handle = NULL;
	WrenHandle* package_keys_handle = NULL;
	size_t package_keys_len = 0;

	char* CLEANUP_STR cwd = NULL;

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

	// remove package staging directory if it still exists

	remove_recursive(staging_path);

	// create package staging directory

	if (mkdir_recursive(staging_path) < 0) {
		LOG_FATAL("Can't create package staging path: mkdir(\"%s\"): %s", staging_path, strerror(errno))
		goto err;
	}

	// remember previous working directory

	cwd = getcwd(NULL, 0);

	if (!cwd) {
		LOG_FATAL("getcwd: %s", strerror(errno))
		goto err;
	}

	// change to package staging directory

	if (chdir(staging_path) < 0) {
		LOG_FATAL("chdir(\"%s\"): %s", staging_path, strerror(errno))
		goto err;
	}

	// format specific functions structure

	size_t const fn_count = sizeof(FORMAT_LUT) / sizeof(*FORMAT_LUT);

	if ((size_t) format > fn_count) {
		LOG_FATAL("Sanity check failed: there are %zu format info structures, but format is %d", fn_count, format)
		goto err;
	}

	format_info_t const* const info = &FORMAT_LUT[format];

	// stage population

	if (info->stage_populate(package) < 0)
		goto err;

	// change back to working directory

	if (chdir(cwd) < 0) {
		LOG_FATAL("chdir(\"%s\"): %s", cwd, strerror(errno))
		goto err;
	}

	free(cwd);
	cwd = NULL; // so that we don't attempt to change back into directory

	// install files to staging directory

	rv = install(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	// stage packaging

	if (info->stage_package(package, staging_path, out) < 0)
		goto err;

	// link output file to bin directory

	remove(link_name);

	if (link(out, link_name) < 0) {
		LOG_FATAL("link(\"%s\", \"%s\"): %s", out, link_name, strerror(errno))
		goto err;
	}

	// success!

	LOG_SUCCESS("Package created at '%s'", out)

err:

	if (cwd)
		chdir(cwd);

	if (package_keys_handle)
		wrenReleaseHandle(state.vm, package_keys_handle);

	if (package_map_handle)
		wrenReleaseHandle(state.vm, package_map_handle);

	wren_clean_vm(&state);

	return rv;
}
