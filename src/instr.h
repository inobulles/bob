#pragma once

#include "base/base.h"

#include "classes/cc.h"
#include "classes/file.h"
#include "classes/linker.h"
#include "util.h"
#include "wren/include/wren.h"
#include <unistd.h>

static WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* signature) {
	WrenForeignMethodFn fn = unknown_foreign;

	// classes

	if (!strcmp(class, "CC")) {
		fn = cc_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "Deps")) {
		fn = deps_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "File")) {
		fn = file_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "Linker")) {
		fn = linker_bind_foreign_method(static_, signature);
	}

	// unknown

	if (fn == unknown_foreign) {
		LOG_WARN("Unknown%s foreign method '%s' in module '%s', class '%s'", static_ ? " static" : "", signature, module, class)
	}

	return fn;
}

static WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class) {
	WrenForeignClassMethods meth = { NULL };

	if (!strcmp(class, "CC")) {
		meth.allocate = cc_new;
		meth.finalize = cc_del;
	}

	else if (!strcmp(class, "Linker")) {
		meth.allocate = linker_new;
		meth.finalize = linker_del;
	}

	else {
		LOG_WARN("Unknown foreign class '%s' in module '%s'", class, module)
	}

	return meth;
}

typedef struct {
	WrenConfiguration config;
	WrenVM* vm;

	// configuration file

	FILE* fp;
	char* src;
} state_t;

static int wren_setup_vm(state_t* state) {
	// setup wren virtual machine

	wrenInitConfiguration(&state->config);

	state->config.writeFn = &wren_write_fn;
	state->config.errorFn = &wren_error_fn;

	state->config.bindForeignMethodFn = &wren_bind_foreign_method;
	state->config.bindForeignClassFn  = &wren_bind_foreign_class;

	state->vm = wrenNewVM(&state->config);

	// read configuration base file

	char const* const module = "main";
	WrenInterpretResult result = wrenInterpret(state->vm, module, base_src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Configuration base ran successfully")
	}

	// read configuration file

	state->fp = fopen("build.wren", "r");

	if (!state->fp) {
		LOG_FATAL("Couldn't read 'build.wren'")
		return EXIT_FAILURE;
	}

	fseek(state->fp, 0, SEEK_END);
	size_t const bytes = ftell(state->fp);
	rewind(state->fp);

	state->src = malloc(bytes);

	if (fread(state->src, 1, bytes, state->fp))
		;

	state->src[bytes - 1] = 0;

	// run configuration file

	result = wrenInterpret(state->vm, module, state->src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Build configuration ran successfully")
	}

	return EXIT_SUCCESS;
}

static int wren_call(state_t* state, char const* class, char const* signature, int argc, char** argv) {
	// get class handle

	if (!wrenHasVariable(state->vm, "main", class)) {
		LOG_ERROR("No '%s' class", class)
		return EXIT_FAILURE;
	}

	wrenEnsureSlots(state->vm, 3);
	wrenGetVariable(state->vm, "main", class, 0);
	WrenHandle* const class_handle = wrenGetSlotHandle(state->vm, 0);

	// get method handle

	WrenHandle* const method_handle = wrenMakeCallHandle(state->vm, signature);
	wrenReleaseHandle(state->vm, class_handle);

	// pass argument list as first argument (if there is one)
	// don't check for argc, because that only tells us if the list is empty - argv tells us if it exists

	if (argv) {
		wrenSetSlotNewList(state->vm, 1);

		while (argc --> 0) {
			wrenSetSlotString(state->vm, 2, *argv++);
			wrenInsertInList(state->vm, 1, -1, 2);
		}
	}

	// actually call function

	WrenInterpretResult const rv = wrenCall(state->vm, method_handle);
	wrenReleaseHandle(state->vm, method_handle);

	// error checking & return value

	if (rv != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running")
		return EXIT_FAILURE;
	}

	if (wrenGetSlotType(state->vm, 0) != WREN_TYPE_NUM) {
		LOG_WARN("Expected number as a return value")
		return EXIT_FAILURE;
	}

	double const _rv = wrenGetSlotDouble(state->vm, 0);
	return _rv;
}

static void wren_clean_vm(state_t* state) {
	if (state->vm) {
		wrenFreeVM(state->vm);
	}

	if (state->src) {
		free(state->src);
	}

	if (state->fp) {
		fclose(state->fp);
	}
}

static int do_build(void) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

err:

	wren_clean_vm(&state);
	return rv;
}

#define ENV_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define ENV_PATH "PATH"

static void setup_env(char* working_dir) {
	char* const env_lib_path = getenv(ENV_LD_LIBRARY_PATH);
	char* const env_bin_path = getenv(ENV_PATH);

	char* lib_path;
	char* path;

	// format new library & binary search paths

	if (!env_lib_path) {
		lib_path = strdup(bin_path);
	}

	else if (asprintf(&lib_path, "%s:%s", env_lib_path, bin_path))
		;

	if (!env_bin_path) {
		path = strdup(bin_path);
	}

	else if (asprintf(&path, "%s:%s", env_bin_path, bin_path))
		;

	// set environment variables

	if (setenv(ENV_LD_LIBRARY_PATH, lib_path, true) < 0) {
		errx(EXIT_FAILURE, "setenv(\"" ENV_LD_LIBRARY_PATH "\", \"%s\"): %s", lib_path, strerror(errno));
	}

	if (setenv(ENV_PATH, path, true) < 0) {
		errx(EXIT_FAILURE, "setenv(\"" ENV_PATH "\", \"%s\"): %s", path, strerror(errno));
	}

	// move into working directory

	if (chdir(working_dir) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", working_dir, strerror(errno));
	}
}

static int do_run(int argc, char** argv) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// setup environment for running
	// TODO ideally this should happen exclusively in a child process, but I think that would be quite complicated to implement

	setup_env(bin_path);

	// call the run function

	rv = wren_call(&state, "Runner", "run(_)", argc, argv);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

err:

	wren_clean_vm(&state);
	return rv;
}

static int do_install(void) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// read installation map

	if (!wrenHasVariable(state.vm, "main", "install")) {
		LOG_ERROR("No installation map")

		rv = EXIT_FAILURE;
		goto err;
	}

	wrenEnsureSlots(state.vm, 4); // first slot for the receiver (ends up being the keys list), second slot for the key, third slot for the value, and last slot as a temporary working slot
	wrenGetVariable(state.vm, "main", "install", 0);

	if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_MAP) {
		LOG_ERROR("'install' variable is not a map")

		rv = EXIT_FAILURE;
		goto err;
	}

	size_t const installation_map_len = wrenGetMapCount(state.vm, 0);

	// keep handle to installation map

	WrenHandle* const map_handle = wrenGetSlotHandle(state.vm, 0);

	// run the 'keys' method on the map

	WrenHandle* const keys_handle = wrenMakeCallHandle(state.vm, "keys");
	WrenInterpretResult const keys_result = wrenCall(state.vm, keys_handle); // no need to set receiver - it's already in slot 0
	wrenReleaseHandle(state.vm, keys_handle);

	if (keys_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'keys' method on the installation map")

		rv = EXIT_FAILURE;
		goto err;
	}

	// run the 'toList' method on the 'MapKeySequence' object

	WrenHandle* const to_list_handle = wrenMakeCallHandle(state.vm, "toList");
	WrenInterpretResult const to_list_result = wrenCall(state.vm, to_list_handle);
	wrenReleaseHandle(state.vm, to_list_handle);

	if (to_list_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'toList' method on the installation map keys' 'MapKeySequence'")

		rv = EXIT_FAILURE;
		goto err;
	}

	// small sanity check - is the converted keys list as big as the map?

	size_t const keys_len = wrenGetListCount(state.vm, 0);

	if (installation_map_len != keys_len) {
		LOG_ERROR("Installation map is not the same size as converted keys list (%zu vs %zu)", installation_map_len, keys_len)

		rv = EXIT_FAILURE;
		goto err;
	}

	// read key/value pairs

	wrenSetSlotHandle(state.vm, 3, map_handle);

	for (size_t i = 0; i < keys_len; i++) {
		// get key

		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("Installation map element %d key is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
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

		char* src;

		if (asprintf(&src, "%s/%s", bin_path, key))
			;

		if (copy(src, val) != EXIT_SUCCESS) {
			LOG_WARN("Failed to install '%s' -> '%s'", key, val)
		}

		else {
			LOG_SUCCESS("Installed '%s' -> '%s'", key, val)
		}

		free(src);
	}

err:

	if (map_handle) {
		wrenReleaseHandle(state.vm, map_handle);
	}

	wren_clean_vm(&state);
	return rv;
}

typedef struct {
	char* name;
	pid_t pid;
} test_t;

static int do_test(void) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// read test list

	if (!wrenHasVariable(state.vm, "main", "tests")) {
		LOG_ERROR("No test list")

		rv = EXIT_FAILURE;
		goto err;
	}

	wrenEnsureSlots(state.vm, 2); // one slot for the 'tests' list, the other for each element
	wrenGetVariable(state.vm, "main", "tests", 0);

	if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_LIST) {
		LOG_ERROR("'tests' variable is not a list")

		rv = EXIT_FAILURE;
		goto err;
	}

	size_t const test_list_len = wrenGetListCount(state.vm, 0);

	size_t tests_len = 0;
	test_t** tests = NULL;

	for (size_t i = 0; i < test_list_len; i++) {
		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("Test list element %d is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const test_name = wrenGetSlotString(state.vm, 1);

		// actually run test

		pid_t const pid = fork();

		if (!pid) {
			// TODO create testing environment

			// setup testing environment
			// TODO change into testing directory/setup testing environment properly

			setup_env(bin_path);

			// create signature
			// we don't care about freeing anything here because the child process will die eventually anyway

			char* signature;

			if (asprintf(&signature, "%s", test_name))
				;

			// call test function

			_exit(wren_call(&state, "Tests", signature, 0, NULL));
		}

		// add to the tests list

		test_t* test = malloc(sizeof *test);

		test->name = strdup(test_name);
		test->pid = pid;

		tests = realloc(tests, ++tests_len * sizeof *tests);
		tests[tests_len - 1] = test;
	}

	// wait for all test processes to finish

	size_t failed_count = 0;

	for (size_t i = 0; i < tests_len; i++) {
		test_t* const test = tests[i];
		int const result = wait_for_process(test->pid);

		if (result == EXIT_SUCCESS) {
			LOG_SUCCESS("Test '%s' passed", test->name)
		}

		else {
			LOG_ERROR("Test '%s' failed (error code %d)", test->name, result)
			failed_count++;
		}

		// free test because we won't be needing it anymore

		if (test->name) {
			free(test->name);
		}

		free(test);
	}

	free(tests);

	// show results

	if (!failed_count) {
		LOG_SUCCESS("All %d tests passed!", tests_len)
	}

	else {
		LOG_ERROR("%d out of %d tests failed", failed_count, tests_len)
	}

err:

	wren_clean_vm(&state);
	return rv;
}
