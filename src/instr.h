#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <wren.h>

#include "base/base.h"

#include "classes/cc.h"
#include "classes/linker.h"

#include "classes/deps.h"
#include "classes/file.h"
#include "classes/meta.h"
#include "classes/resources.h"

#include "logging.h"
#include "util.h"

static WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* sig) {
	WrenForeignMethodFn fn = unknown_foreign;

	// object classes

	if (!strcmp(class, "CC")) {
		fn = cc_bind_foreign_method(static_, sig);
	}

	else if (!strcmp(class, "Linker")) {
		fn = linker_bind_foreign_method(static_, sig);
	}

	// static classes

	else if (!strcmp(class, "Deps")) {
		fn = deps_bind_foreign_method(static_, sig);
	}

	else if (!strcmp(class, "File")) {
		fn = file_bind_foreign_method(static_, sig);
	}

	else if (!strcmp(class, "Meta")) {
		fn = meta_bind_foreign_method(static_, sig);
	}

	else if (!strcmp(class, "Resources")) {
		fn = resources_bind_foreign_method(static_, sig);
	}

	// unknown

	if (fn == unknown_foreign) {
		LOG_WARN("Unknown%s foreign method '%s' in module '%s', class '%s'", static_ ? " static" : "", sig, module, class)
	}

	return fn;
}

static WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class) {
	WrenForeignClassMethods meth = { 0 };

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

	if (result != WREN_RESULT_SUCCESS) {
		LOG_FATAL("Something went wrong executing configuration base")
		return EXIT_FAILURE;
	}

	// read configuration file
	// TODO can the filepointer be closed right after reading?

	state->fp = fopen("build.wren", "r");

	if (!state->fp) {
		LOG_FATAL("Couldn't read 'build.wren'")
		return EXIT_FAILURE;
	}

	size_t const size = file_get_size(state->fp);
	state->src = file_read_str(state->fp, size);

	// run configuration file

	result = wrenInterpret(state->vm, module, state->src);

	if (result != WREN_RESULT_SUCCESS) {
		LOG_FATAL("Something wrong executing build configuration")
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int wren_call(state_t* state, char const* class, char const* sig, char const** str_ref) {
	// check class exists

	if (!wrenHasVariable(state->vm, "main", class)) {
		LOG_ERROR("No '%s' class", class)
		return EXIT_FAILURE;
	}

	// get class handle

	wrenEnsureSlots(state->vm, 1);
	wrenGetVariable(state->vm, "main", class, 0);
	WrenHandle* const class_handle = wrenGetSlotHandle(state->vm, 0);

	// get method handle

	WrenHandle* const method_handle = wrenMakeCallHandle(state->vm, sig);
	wrenReleaseHandle(state->vm, class_handle);

	// actually call function

	WrenInterpretResult const rv = wrenCall(state->vm, method_handle);
	wrenReleaseHandle(state->vm, method_handle);

	// error checking & return value

	if (rv != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running")
		return EXIT_FAILURE;
	}

	if (wrenGetSlotType(state->vm, 0) == WREN_TYPE_NUM) {
		double const _rv = wrenGetSlotDouble(state->vm, 0);
		return _rv;
	}

	else if (wrenGetSlotType(state->vm, 0) == WREN_TYPE_BOOL) {
		return !wrenGetSlotBool(state->vm, 0);
	}

	else if (str_ref && wrenGetSlotType(state->vm, 0) == WREN_TYPE_STRING) {
		char const* const _str = wrenGetSlotString(state->vm, 0);
		*str_ref = _str;

		return _str ? EXIT_SUCCESS : EXIT_FAILURE;
	}


	LOG_WARN("Expected number or boolean as a return value")
	return EXIT_FAILURE;
}

static int wren_call_args(state_t* state, char const* class, char const* sig, int argc, char** argv) {
	// pass argument list as first argument

	wrenEnsureSlots(state->vm, 3);
	wrenSetSlotNewList(state->vm, 1);

	while (argc --> 0) {
		wrenSetSlotString(state->vm, 2, *argv++);
		wrenInsertInList(state->vm, 1, -1, 2);
	}

	// actually call

	return wren_call(state, class, sig, NULL);
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

	else if (asprintf(&lib_path, "%s:%s", env_lib_path, bin_path)) {}

	if (!env_bin_path) {
		path = strdup(bin_path);
	}

	else if (asprintf(&path, "%s:%s", env_bin_path, bin_path)) {}

	// set environment variables

	if (setenv(ENV_LD_LIBRARY_PATH, lib_path, true) < 0) {
		errx(EXIT_FAILURE, "setenv(\"" ENV_LD_LIBRARY_PATH "\", \"%s\"): %s", lib_path, strerror(errno));
	}

	if (setenv(ENV_PATH, path, true) < 0) {
		errx(EXIT_FAILURE, "setenv(\"" ENV_PATH "\", \"%s\"): %s", path, strerror(errno));
	}

	// move into working directory

	if (working_dir && chdir(working_dir) < 0) {
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

	rv = wren_call_args(&state, "Runner", "run(_)", argc, argv);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

err:

	wren_clean_vm(&state);

	return rv;
}

static int read_installation_map(state_t* state, WrenHandle** map_handle_ref, size_t* keys_len_ref) {
	int rv = EXIT_SUCCESS;

	// read installation map

	if (!wrenHasVariable(state->vm, "main", "install")) {
		LOG_ERROR("No installation map")

		rv = EXIT_FAILURE;
		goto err;
	}

	wrenEnsureSlots(state->vm, 1); // for the receiver (starts off as the installation map, ends up being the keys list)
	wrenGetVariable(state->vm, "main", "install", 0);

	if (wrenGetSlotType(state->vm, 0) != WREN_TYPE_MAP) {
		LOG_ERROR("'install' variable is not a map")

		rv = EXIT_FAILURE;
		goto err;
	}

	size_t const installation_map_len = wrenGetMapCount(state->vm, 0);

	// keep handle to installation map

	if (map_handle_ref) {
		*map_handle_ref = wrenGetSlotHandle(state->vm, 0);
	}

	// run the 'keys' method on the map

	WrenHandle* const keys_handle = wrenMakeCallHandle(state->vm, "keys");
	WrenInterpretResult const keys_result = wrenCall(state->vm, keys_handle); // no need to set receiver - it's already in slot 0
	wrenReleaseHandle(state->vm, keys_handle);

	if (keys_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'keys' method on the installation map")

		rv = EXIT_FAILURE;
		goto err;
	}

	// run the 'toList' method on the 'MapKeySequence' object

	WrenHandle* const to_list_handle = wrenMakeCallHandle(state->vm, "toList");
	WrenInterpretResult const to_list_result = wrenCall(state->vm, to_list_handle);
	wrenReleaseHandle(state->vm, to_list_handle);

	if (to_list_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'toList' method on the installation map keys' 'MapKeySequence'")

		rv = EXIT_FAILURE;
		goto err;
	}

	// small sanity check - is the converted keys list as big as the map?

	size_t const keys_len = wrenGetListCount(state->vm, 0);

	if (installation_map_len != keys_len) {
		LOG_ERROR("Installation map is not the same size as converted keys list (%zu vs %zu)", installation_map_len, keys_len)

		rv = EXIT_FAILURE;
		goto err;
	}

	if (keys_len_ref) {
		*keys_len_ref = keys_len;
	}

err:

	return rv;
}

static int do_install(void) {
	// declarations which must come before first goto

	WrenHandle* map_handle = NULL;
	WrenHandle* keys_handle = NULL;
	size_t keys_len = 0;

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	rv = read_installation_map(&state, &map_handle, &keys_len);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

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

		if (*val != ':') {
			goto install;
		}

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

		if (!basename) {
			basename = dest_parent;
		}

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

	if (keys_handle) {
		wrenReleaseHandle(state.vm, keys_handle);
	}

	if (map_handle) {
		wrenReleaseHandle(state.vm, map_handle);
	}

	wren_clean_vm(&state);

	return rv;
}

typedef struct {
	char* name;
	pid_t pid;

	int result;
	pipe_t pipe;
} test_t;

static int do_test(void) {
	// declarations which must come before first goto

	char** keys = NULL;

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	// TODO recursively test? when and when not to?

	// get list of installation map keys
	// this will tell us which files we need to copy for each testing environment
	// we don't wanna copy everything, because there might be a lot of shit in the output directory!

	WrenHandle* map_handle = NULL;
	size_t keys_len = 0;

	rv = read_installation_map(&state, &map_handle, &keys_len);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	wrenEnsureSlots(state.vm, 2); // first for the key list itself, second for the keys
	keys = calloc(keys_len, sizeof *keys);

	for (size_t i = 0; i < keys_len; i++) {
		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("Installation map key %zu is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		const char* const key = wrenGetSlotString(state.vm, 1);
		keys[i] = strdup(key);
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

	// actually run tests

	size_t tests_len = 0;
	test_t** tests = NULL;

	for (size_t i = 0; i < test_list_len; i++) {
		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("Test list element %zu is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const test_name = wrenGetSlotString(state.vm, 1);

		// setup pipes

		pipe_t pipe = { .kind = PIPE_STDOUT | PIPE_STDERR };
		pipe_create(&pipe);

		// actually run test

		pid_t const pid = fork();

		if (!pid) {
			pipe_child(&pipe);

			// in here, we don't care about freeing anything because the child process will die eventually anyway
			// create testing environment by first creating the test directory

			uint64_t const hash = hash_str(test_name);

			char* test_dir;
			if (asprintf(&test_dir, "%s/%lx", bin_path, hash)) {}

			// remove test directory if it already exists

			remove_recursive(test_dir);

			// copy over test directory
			// create it if it doesn't yet exist
			// TODO don't hardcode the prefix - should be defined by a variable in the base configuration instead

			char* test_files_dir;
			if (asprintf(&test_files_dir, "tests/%s", test_name)) {}

			if (!access(test_files_dir, W_OK)) {
				copy_recursive(test_files_dir, test_dir);
			}

			else if (mkdir(test_dir, 0777) < 0 && errno != EEXIST) {
				errx(EXIT_FAILURE, "mkdir(\"%s\"): %s", test_dir, strerror(errno));
			}

			// setup testing environment

			setup_env(test_dir);

			// then, copy over all the files in 'keys'

			for (size_t i = 0; i < keys_len; i++) {
				char* const key = keys[i];

				char* src;
				if (asprintf(&src, "%s/%s", bin_path, key)) {}

				copy_recursive(src, key);
				free(src);
			}

			// create signature

			char* sig;
			if (asprintf(&sig, "%s", test_name)) {}

			// call test function

			_exit(wren_call(&state, "Tests", sig, NULL));
		}

		pipe_parent(&pipe);

		// add to the tests list

		test_t* test = malloc(sizeof *test);

		test->name = strdup(test_name);
		test->pid = pid;
		test->pipe = pipe;

		tests = realloc(tests, ++tests_len * sizeof *tests);
		tests[tests_len - 1] = test;
	}

	// wait for all test processes to finish

	progress_t* const progress = progress_new();
	size_t failed_count = 0;

	for (size_t i = 0; i < tests_len; i++) {
		test_t* const test = tests[i];

		progress_update(progress, i, test_list_len, "Running test '%s' (%zu of %zu, %zu failed)", test->name, i + 1, test_list_len, failed_count);
		test->result = wait_for_process(test->pid);

		if (test->result != EXIT_SUCCESS) {
			failed_count++;
		}
	}

	// complete progress

	progress_complete(progress);
	progress_del(progress);

	// print out test output

	for (size_t i = 0; i < tests_len; i++) {
		test_t* const test = tests[i];

		if (test->result == EXIT_SUCCESS) {
			goto succeeded;
		}

		char* const out = pipe_read_out(&test->pipe, PIPE_STDOUT);
		char* const err = pipe_read_out(&test->pipe, PIPE_STDERR);

		bool const has_out = *out || *err;

		LOG_ERROR("Test '%s' failed%s", test->name, has_out ? ":" : "")

		fprintf(stdout, "%s", out);
		fprintf(stderr, "%s", err);

		free(out);
		free(err);

	succeeded:

		// free test because we won't be needing it anymore

		if (test->name) {
			free(test->name);
		}

		pipe_free(&test->pipe);
		free(test);
	}

	free(tests);

	// show results

	if (!tests_len) {
		LOG_WARN("No tests to run")
	}

	else if (!failed_count) {
		LOG_SUCCESS("All %zu tests passed", tests_len)
	}

	else {
		rv = EXIT_FAILURE;
		LOG_ERROR("%zu out of %zu tests failed", failed_count, tests_len)
	}

err:

	for (size_t i = 0; keys && i < keys_len; i++) {
		char* const key = keys[i];

		if (key) {
			free(key);
		}
	}

	if (keys) {
		free(keys);
	}

	if (map_handle) {
		wrenReleaseHandle(state.vm, map_handle);
	}

	wren_clean_vm(&state);

	return rv;
}
