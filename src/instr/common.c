#include <instr.h>

#include <classes/package_t.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define ENV_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define ENV_PATH "PATH"

int setup_env(char* working_dir) {
	char* const env_lib_path = getenv(ENV_LD_LIBRARY_PATH);
	char* const env_bin_path = getenv(ENV_PATH);

	char* CLEANUP_STR lib_path;
	char* CLEANUP_STR path;

	// format new library & binary search paths

	if (!env_lib_path) {
		lib_path = strdup(bin_path);
	}

	else if (asprintf(&lib_path, "%s:%s", env_lib_path, bin_path)) {}

	if (!env_bin_path) {
		path = strdup(bin_path);
	}

	else if (asprintf(&path, "%s:%s", env_bin_path, bin_path)) {}

	// set environment variables

	if (setenv(ENV_LD_LIBRARY_PATH, lib_path, true) < 0) {
		LOG_FATAL("setenv(\"" ENV_LD_LIBRARY_PATH "\", \"%s\"): %s", lib_path, strerror(errno))
		return -1;
	}

	if (setenv(ENV_PATH, path, true) < 0) {
		LOG_FATAL("setenv(\"" ENV_PATH "\", \"%s\"): %s", path, strerror(errno))
		return -1;
	}

	// move into working directory

	if (working_dir && chdir(working_dir) < 0) {
		LOG_FATAL("chdir(\"%s\"): %s", working_dir, strerror(errno))
		return -1;
	}

	return 0;
}

int install(state_t* state) {
	// declarations which must come before first goto

	WrenHandle* map_handle = NULL;
	WrenHandle* keys_handle = NULL;
	size_t keys_len = 0;

	// read installation map

	int rv = wren_read_map(state, INSTALL_MAP, &map_handle, &keys_len);

	if (rv != EXIT_SUCCESS)
		goto err;

	// read key/value pairs

	keys_handle = wrenGetSlotHandle(state->vm, 0);

	wrenEnsureSlots(state->vm, 4); // first slot for the keys list, second for the key, third slot for the value, last for map
	wrenSetSlotHandle(state->vm, 3, map_handle);

	progress_t* const progress = progress_new();

	for (size_t i = 0; i < keys_len; i++) {
		// this is declared all the way up here, because you can't jump over an __attribute__((cleanup)) declaration with goto

		char* CLEANUP_STR sig = NULL;

		// get key

		wrenGetListElement(state->vm, 0, i, 1);

		if (wrenGetSlotType(state->vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("'%s' map element %zu key is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", INSTALL_MAP, i)
			continue;
		}

		char const* const key = wrenGetSlotString(state->vm, 1);

		// sanity check - make sure map contains key

		if (!wrenGetMapContainsKey(state->vm, 3, 1)) {
			LOG_WARN("'%s' map does not contain key '%s'", INSTALL_MAP, key)
			continue;
		}

		// get value

		wrenGetMapValue(state->vm, 3, 1, 2);

		if (wrenGetSlotType(state->vm, 2) != WREN_TYPE_STRING) {
			LOG_WARN("'%s' map element '%s' value is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", INSTALL_MAP, key)
			continue;
		}

		char const* const val = wrenGetSlotString(state->vm, 2);
		char const* dest = val;

		char* CLEANUP_STR src = NULL;
		if (asprintf(&src, "%s/%s", bin_path, key)) {}

		char* CLEANUP_STR orig_dest_prefix = NULL;
		if (asprintf(&orig_dest_prefix, "%s/%s", install_prefix(), dest)) {}

		char* dest_prefix = orig_dest_prefix;

		// execute installer method if there is one
		// otherwise, just install

		if (*val != ':')
			goto install;

		dest_prefix = NULL;

		if (!wrenHasVariable(state->vm, "main", "Installer")) {
			LOG_WARN("'%s' is installed to '%s', which starts with a colon - this is normally used for installer methods, but module has no 'Installer' class", INSTALL_MAP, key, val)
			goto install;
		}

		// call installer method

		if (asprintf(&sig, "%s(_)", val + 1)) {}

		wrenEnsureSlots(state->vm, 2);
		wrenSetSlotString(state->vm, 1, src);

		if (wren_call(state, "Installer", sig, &dest) != EXIT_SUCCESS) {
			progress_complete(progress);
			progress_del(progress);

			LOG_ERROR("'%s' method for '%s' failed", INSTALL_MAP, key)
			goto err;
		}

		// reset slots from handles, because wren_call may have mangled all of this

		wrenSetSlotHandle(state->vm, 0, keys_handle);
		wrenSetSlotHandle(state->vm, 3, map_handle);

		// install file/directory

	install:

		progress_update(progress, i, keys_len, "Installing '%s' to '%s' (%d of %d)", key, dest, i + 1, keys_len);

		// make sure the directory in which we'd like to install the file/directory exists
		// then, copy the file/directory itself

		if (dest_prefix)
			dest = dest_prefix;

		char* const CLEANUP_STR dest_parent = strdup(dest);
		char* basename = strrchr(dest_parent, '/');

		if (!basename)
			basename = dest_parent;

		*basename = '\0';

		if (mkdir_recursive(dest_parent) < 0) {
			progress_complete(progress);
			progress_del(progress);

			LOG_ERROR("Failed to install '%s' -> '%s' (mkdir): %s", key, dest, strerror(errno))
			goto err;
		}

		char* const CLEANUP_STR copy_err = copy_recursive(src, dest);

		if (copy_err != NULL) {
			progress_complete(progress);
			progress_del(progress);

			LOG_ERROR("Failed to install '%s' -> '%s' (copy): %s", key, dest, copy_err)
			goto err;
		}
	}

	// finished!

	progress_complete(progress);
	progress_del(progress);

	LOG_SUCCESS("All %zu files installed", keys_len)

err:

	if (keys_handle)
		wrenReleaseHandle(state->vm, keys_handle);

	if (map_handle)
		wrenReleaseHandle(state->vm, map_handle);

	return rv;
}

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

	if (execute(exec_args) == EXIT_FAILURE) {
		rv = -1;
	}

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

static format_t get_format(char const* format) {
	if (!strcmp(format, "zpk")) {
		return FORMAT_ZPK;
	}

	// if (!strcmp(format, "fbsd"))
	// 	return FORMAT_FBSD;

	return FORMAT_UNSUPPORTED;
}

int package(state_t* state, char const* format_str, char* name, char* _out) {
	format_t const format = get_format(format_str);

	if (format == FORMAT_UNSUPPORTED) {
		LOG_ERROR("Format '%s' is unsupported", format_str)
		return EXIT_FAILURE;
	}

	char* CLEANUP_STR out = NULL;

	if (_out == NULL) {
		if (asprintf(&out, "%s.%s", name, format_str)) {}
	}

	else {
		out = strdup(_out);
	}

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

	bool change_back = true;
	char* CLEANUP_STR cwd = NULL;
	char* CLEANUP_STR cwd2 = NULL;

	int rv = wren_read_map(state, PACKAGE_MAP, &package_map_handle, &package_keys_len);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// read package map key/value pairs
	// look for the package name asked of us

	char const* key = NULL;

	package_keys_handle = wrenGetSlotHandle(state->vm, 0);

	wrenEnsureSlots(state->vm, 4); // first slot for the keys list, second for the key, third slot for the value, last for map
	wrenSetSlotHandle(state->vm, 3, package_map_handle);

	for (size_t i = 0; i < package_keys_len; i++) {
		// get key

		wrenGetListElement(state->vm, 0, i, 1);

		if (wrenGetSlotType(state->vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("'%s' map element %zu key is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", PACKAGE_MAP, i)
			continue;
		}

		key = wrenGetSlotString(state->vm, 1);

		// sanity check - make sure map contains key

		if (!wrenGetMapContainsKey(state->vm, 3, 1)) {
			LOG_WARN("'%s' map does not contain key '%s'", PACKAGE_MAP, key)
			continue;
		}

		// is the key what we're looking for?

		if (!strcmp(key, name)) {
			goto found;
		}
	}

	// found nothing :'(

	LOG_FATAL("'%s' map doesn't contain package '%s'", PACKAGE_MAP, name)

found:

	// get value

	wrenGetMapValue(state->vm, 3, 1, 2);

	if (wrenGetSlotType(state->vm, 2) != WREN_TYPE_FOREIGN) {
		LOG_FATAL("'%s' map element '%s' value is of incorrect type (expected 'WREN_TYPE_FOREIGN')", PACKAGE_MAP, key)
		goto err;
	}

	package_t* const package = wrenGetSlotForeign(state->vm, 2);

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

	if (info->stage_populate(package) < 0) {
		goto err;
	}

	// change back to working directory

	if (chdir(cwd) < 0) {
		LOG_FATAL("chdir(\"%s\"): %s", cwd, strerror(errno))
		goto err;
	}

	change_back = false;

	// install files to staging directory

	rv = install(state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// stage packaging

	if (info->stage_package(package, staging_path, out) < 0) {
		goto err;
	}

	// link output file to bin directory
	// only do this is we're not in the bin directory though

	cwd2 = getcwd(NULL, 0);

	if (!cwd2 || strcmp(cwd2, cwd) != 0) {
		remove(link_name);

		if (link(out, link_name) < 0) {
			LOG_FATAL("link(\"%s\", \"%s\"): %s", out, link_name, strerror(errno))
			goto err;
		}
	}

	// success!

	LOG_SUCCESS("Package created at '%s'", out)

err:

	if (change_back) {
		chdir(cwd);
	}

	if (package_keys_handle) {
		wrenReleaseHandle(state->vm, package_keys_handle);
	}

	if (package_map_handle) {
		wrenReleaseHandle(state->vm, package_map_handle);
	}

	return rv;
}
