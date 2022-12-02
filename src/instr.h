#pragma once

#include "base/base.h"

#include "classes/cc.h"
#include "classes/file.h"
#include "classes/linker.h"
#include "util.h"
#include <sys/types.h>
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

static int wren_call(state_t* state, char const* name) {
	if (!wrenHasVariable(state->vm, "main", name)) {
		LOG_ERROR("No run function")
		return EXIT_FAILURE;
	}

	wrenEnsureSlots(state->vm, 1);
	wrenGetVariable(state->vm, "main", "run", 0);
	WrenHandle* const handle = wrenGetSlotHandle(state->vm, 0);

	WrenInterpretResult const rv = wrenCall(state->vm, handle);
	wrenReleaseHandle(state->vm, handle);

	if (rv != WREN_RESULT_SUCCESS) {
		LOG_WARN("Something went wrong running")
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
		goto error;
	}

error:

	wren_clean_vm(&state);
	return rv;
}

static int do_run(void) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto error;
	}

	// setup environment for running
	// TODO ideally this should happen exclusively in a child process, but I think that would be quite complicated to implement

	if (setenv("LD_LIBRARY_PATH", bin_path, true) < 0) {
		errx(EXIT_FAILURE, "setenv(\"LD_LIBRARY_PATH\", \"%s\"): %s", bin_path, strerror(errno));
	}

	if (chdir(bin_path) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", bin_path, strerror(errno));
	}

	// call the run function

	rv = wren_call(&state, "run");

	if (rv != EXIT_SUCCESS) {
		goto error;
	}

error:

	wren_clean_vm(&state);
	return rv;
}

static int do_test(void) {
	state_t state = { 0 };
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto error;
	}

	// read all tests

	if (!wrenHasVariable(state.vm, "main", "tests")) {
		LOG_ERROR("No test list")

		rv = EXIT_FAILURE;
		goto error;
	}

	wrenEnsureSlots(state.vm, 2); // one slot for the 'tests' list, the other for each element
	wrenGetVariable(state.vm, "main", "tests", 0);

	if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_LIST) {
		LOG_ERROR("'tests' variable is not a list")

		rv = EXIT_FAILURE;
		goto error;
	}

	size_t const test_list_len = wrenGetListCount(state.vm, 0);

	size_t test_processes_len = 0;
	pid_t* test_processes = NULL;

	for (size_t i = 0; i < test_list_len; i++) {
		wrenGetListElement(state.vm, 0, i, 1);

		// I don't know of a way to check if we're dealing with a function, so this is as good as we're gonna get for now

		if (wrenGetSlotType(state.vm, 1) != WREN_TYPE_UNKNOWN) {
			LOG_WARN("Test list element at index %d is not a function - skipping")
			continue;
		}

		// actually run test

		WrenHandle* const handle = wrenGetSlotHandle(state.vm, 1);

		pid_t const pid = fork();

		if (!pid) {
			// setup environment

			if (setenv("LD_LIBRARY_PATH", bin_path, true) < 0) {
				errx(EXIT_FAILURE, "setenv(\"LD_LIBRARY_PATH\", \"%s\"): %s", bin_path, strerror(errno));
			}

			if (chdir(bin_path) < 0) { // TODO change into testing directory
				errx(EXIT_FAILURE, "chdir(\"%s\"): %s", bin_path, strerror(errno));
			}

			// call test function

			WrenInterpretResult const rv = wrenCall(state.vm, handle);

			if (rv != WREN_RESULT_SUCCESS) {
				LOG_WARN("Something went wrong running one of the tests");
				_exit(EXIT_FAILURE);
			}

			if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_NUM) {
				LOG_WARN("Expected number as return value")
				_exit(EXIT_FAILURE);
			}

			double const _rv = wrenGetSlotDouble(state.vm, 0);
			_exit(_rv);
		}

		wrenReleaseHandle(state.vm, handle);

		test_processes = realloc(test_processes, ++test_processes_len * sizeof *test_processes);
		test_processes[test_processes_len - 1] = pid;
	}

	// wait for all test processes to finish

	size_t failed_count = 0;

	for (size_t i = 0; i < test_processes_len; i++) {
		pid_t const pid = test_processes[i];
		int const test_result = wait_for_process(pid);
		failed_count += test_result != EXIT_SUCCESS;
	}

	free(test_processes);

	if (!failed_count) {
		LOG_SUCCESS("All %d tests passed!", test_processes_len)
	}

	else {
		LOG_ERROR("%d out of %d tests failed", failed_count, test_processes_len)
	}

error:

	wren_clean_vm(&state);
	return rv;
}
