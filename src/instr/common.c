#include <instr.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define ENV_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define ENV_PATH "PATH"

void setup_env(char* working_dir) {
	char* const env_lib_path = getenv(ENV_LD_LIBRARY_PATH);
	char* const env_bin_path = getenv(ENV_PATH);

	char* lib_path;
	char* path;

	// format new library & binary search paths

	if (!env_lib_path)
		lib_path = strdup(bin_path);

	else if (asprintf(&lib_path, "%s:%s", env_lib_path, bin_path)) {}

	if (!env_bin_path)
		path = strdup(bin_path);

	else if (asprintf(&path, "%s:%s", env_bin_path, bin_path)) {}

	// set environment variables

	if (setenv(ENV_LD_LIBRARY_PATH, lib_path, true) < 0)
		errx(EXIT_FAILURE, "setenv(\"" ENV_LD_LIBRARY_PATH "\", \"%s\"): %s", lib_path, strerror(errno));

	if (setenv(ENV_PATH, path, true) < 0)
		errx(EXIT_FAILURE, "setenv(\"" ENV_PATH "\", \"%s\"): %s", path, strerror(errno));

	// move into working directory

	if (working_dir && chdir(working_dir) < 0)
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", working_dir, strerror(errno));
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
