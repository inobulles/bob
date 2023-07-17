// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <instr.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

typedef struct {
	char* name;
	pid_t pid;

	int result;
	pipe_t pipe;
} test_t;

int do_test(void) {
	if (navigate_project_path() < 0) {
		return EXIT_FAILURE;
	}

	if (ensure_out_path() < 0) {
		return EXIT_FAILURE;
	}

	// declarations which must come before first goto

	char** keys = NULL;

	// setup state

	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS)
		goto err;

	// TODO recursively test? when and when not to?

	// get list of installation map keys
	// this will tell us which files we need to copy for each testing environment
	// we don't wanna copy everything, because there might be a lot of shit in the output directory!

	WrenHandle* map_handle = NULL;
	size_t keys_len = 0;

	rv = wren_read_map(&state, INSTALL_MAP, &map_handle, &keys_len);

	if (rv != EXIT_SUCCESS)
		goto err;

	wrenEnsureSlots(state.vm, 2); // first for the key list itself, second for the keys
	keys = calloc(keys_len, sizeof *keys);

	for (size_t i = 0; i < keys_len; i++) {
		wrenGetListElement(state.vm, 0, i, 1);

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_STRING) {
			LOG_WARN("'%s' map key %zu is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", INSTALL_MAP, i)
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

		if (pipe_create(&pipe) < 0) {
			continue;
		}

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
				char* const CLEANUP_STR err = copy_recursive(test_files_dir, test_dir);

				if (err != NULL) {
					LOG_WARN("Failed to copy '%s' to '%s': %s", test_files_dir, test_dir, err)
				}
			}

			else if (mkdir(test_dir, 0777) < 0 && errno != EEXIST) {
				LOG_WARN("mkdir(\"%s\"): %s", test_dir, strerror(errno))
				_exit(EXIT_FAILURE);
			}

			// setup testing environment

			if (setup_env(test_dir) < 0) {
				_exit(EXIT_FAILURE);
			}

			// then, copy over all the files in 'keys'

			for (size_t i = 0; i < keys_len; i++) {
				char* const key = keys[i];

				char* CLEANUP_STR src = NULL;
				if (asprintf(&src, "%s/%s", bin_path, key)) {}

				char* const CLEANUP_STR err = copy_recursive(src, key);

				if (err != NULL) {
					LOG_WARN("Failed to copy '%s' to '%s': %s", src, key, err)
				}
			}

			// create signature

			char* sig;
			if (asprintf(&sig, "%s", test_name)) {}

			// call test function

			_exit(wren_call(&state, "Tests", sig, NULL));
		}

		pipe_parent(&pipe);

		// add to the tests list

		test_t* const test = malloc(sizeof *test);

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

		if (test->result != EXIT_SUCCESS)
			failed_count++;
	}

	// complete progress

	progress_complete(progress);
	progress_del(progress);

	// print out test output

	for (size_t i = 0; i < tests_len; i++) {
		test_t* const test = tests[i];

		if (test->result == EXIT_SUCCESS)
			goto succeeded;

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

		if (test->name)
			free(test->name);

		pipe_free(&test->pipe);
		free(test);
	}

	free(tests);

	// show results

	if (!tests_len)
		LOG_WARN("No tests to run")

	else if (!failed_count)
		LOG_SUCCESS("All %zu tests passed", tests_len)

	else {
		rv = EXIT_FAILURE;
		LOG_ERROR("%zu out of %zu tests failed", failed_count, tests_len)
	}

err:

	for (size_t i = 0; keys && i < keys_len; i++) {
		char* const key = keys[i];

		if (key)
			free(key);
	}

	if (keys)
		free(keys);

	if (map_handle)
		wrenReleaseHandle(state.vm, map_handle);

	wren_clean_vm(&state);

	if (fix_out_path_owner() < 0) {
		return EXIT_FAILURE;
	}

	return rv;
}
