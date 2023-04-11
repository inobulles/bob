#include <instr.h>

#include <string.h>

int do_install(void) {
	navigate_project_path();
	ensure_out_path();

	// declarations which must come before first goto

	WrenHandle* map_handle = NULL;
	WrenHandle* keys_handle = NULL;
	size_t keys_len = 0;

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	rv = read_installation_map(&state, &map_handle, &keys_len);

	if (rv != EXIT_SUCCESS)
		goto err;

	// read key/value pairs

	keys_handle = wrenGetSlotHandle(state.vm, 0);

	wrenEnsureSlots(state.vm, 4); // first slot for the keys list, second for the key, third slot for the value, last for map
	wrenSetSlotHandle(state.vm, 3, map_handle);

	progress_t* const progress = progress_new();

	for (size_t i = 0; i < keys_len; i++) {
		// this is declared all the way up here, because you can't jump over an __attribute__((cleanup)) declaration with goto

		char* __attribute__((cleanup(strfree))) sig = NULL;

		// get key

		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("Installation map element %zu key is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const key = wrenGetSlotString(state.vm, 1);

		// sanity check - make sure map contains key

		if (!wrenGetMapContainsKey(state.vm, 3, 1)) {
			LOG_WARN("Installation map does not contain key '%s'", key)
			continue;
		}

		// get value

		wrenGetMapValue(state.vm, 3, 1, 2);

		if (wrenGetSlotType(state.vm, 2) != WREN_TYPE_STRING) {
			LOG_WARN("Installation map element '%s' value is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", key)
			continue;
		}

		char const* const val = wrenGetSlotString(state.vm, 2);
		char const* dest = val;

		char* __attribute__((cleanup(strfree))) src = NULL;
		if (asprintf(&src, "%s/%s", bin_path, key)) {}

		// execute installer method if there is one
		// otherwise, just install

		// check if there's an installer method we need to call

		if (*val != ':')
			goto install;

		if (!wrenHasVariable(state.vm, "main", "Installer")) {
			LOG_WARN("'%s' is installed to '%s', which starts with a colon - this is normally used for installer methods, but module has no 'Installer' class", key, val)
			goto install;
		}

		// call installer method

		if (asprintf(&sig, "%s(_)", val + 1)) {}

		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotString(state.vm, 1, src);

		if (wren_call(&state, "Installer", sig, &dest) != EXIT_SUCCESS) {
			progress_complete(progress);
			progress_del(progress);

			LOG_ERROR("Installation method for '%s' failed", key)
			goto err;
		}

		// reset slots from handles, because wren_call may have mangled all of this

		wrenSetSlotHandle(state.vm, 0, keys_handle);
		wrenSetSlotHandle(state.vm, 3, map_handle);

		// install file/directory

	install:

		progress_update(progress, i, keys_len, "Installing '%s' to '%s' (%d of %d)", key, dest, i + 1, keys_len);

		// make sure the directory in which we'd like to install the file/directory exists
		// then, copy the file/directory itself

		char* const __attribute__((cleanup(strfree))) dest_parent = strdup(dest);
		char* basename = strrchr(dest_parent, '/');

		if (!basename)
			basename = dest_parent;

		*basename = '\0';

		if (
			mkdir_recursive(dest_parent) < 0 ||
			copy_recursive(src, dest) != EXIT_SUCCESS
		) {
			progress_complete(progress);
			progress_del(progress);

			LOG_ERROR("Failed to install '%s' -> '%s'", key, dest)
			goto err;
		}
	}

	// finished!

	progress_complete(progress);
	progress_del(progress);

	LOG_SUCCESS("All %zu files installed", keys_len)

err:

	if (keys_handle)
		wrenReleaseHandle(state.vm, keys_handle);

	if (map_handle)
		wrenReleaseHandle(state.vm, map_handle);

	wren_clean_vm(&state);

	return rv;
}
