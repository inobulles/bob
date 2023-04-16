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

int read_map(state_t* state, char const* name, WrenHandle** map_handle_ref, size_t* keys_len_ref) {
	int rv = EXIT_SUCCESS;

	// read installation map

	if (!wrenHasVariable(state->vm, "main", name)) {
		LOG_ERROR("No '%s' map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	wrenEnsureSlots(state->vm, 1); // for the receiver (starts off as the map, ends up being the keys list)
	wrenGetVariable(state->vm, "main", name, 0);

	if (wrenGetSlotType(state->vm, 0) != WREN_TYPE_MAP) {
		LOG_ERROR("'%s' variable is not a map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	size_t const map_len = wrenGetMapCount(state->vm, 0);

	// keep handle to installation map

	if (map_handle_ref)
		*map_handle_ref = wrenGetSlotHandle(state->vm, 0);

	// run the 'keys' method on the map

	WrenHandle* const keys_handle = wrenMakeCallHandle(state->vm, "keys");
	WrenInterpretResult const keys_result = wrenCall(state->vm, keys_handle); // no need to set receiver - it's already in slot 0
	wrenReleaseHandle(state->vm, keys_handle);

	if (keys_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'keys' method on the '%s' map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	// run the 'toList' method on the 'MapKeySequence' object

	WrenHandle* const to_list_handle = wrenMakeCallHandle(state->vm, "toList");
	WrenInterpretResult const to_list_result = wrenCall(state->vm, to_list_handle);
	wrenReleaseHandle(state->vm, to_list_handle);

	if (to_list_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'toList' method on the '%s' map keys' 'MapKeySequence'", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	// small sanity check - is the converted keys list as big as the map?

	size_t const keys_len = wrenGetListCount(state->vm, 0);

	if (map_len != keys_len) {
		LOG_ERROR("'%s' map is not the same size as converted keys list (%zu vs %zu)", name, map_len, keys_len)

		rv = EXIT_FAILURE;
		goto err;
	}

	if (keys_len_ref)
		*keys_len_ref = keys_len;

err:

	return rv;
}
