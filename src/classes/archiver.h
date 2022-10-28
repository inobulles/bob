#pragma once

#include "../util.h"

#include "cc.h"

typedef struct {
	cc_t* cc;
	char* path;
} archiver_t;

// constructor/destructor

static void archiver_new(WrenVM* vm) {
	CHECK_ARGC("Archiver.new", 0, 1)

	archiver_t* const archiver = wrenSetSlotNewForeign(vm, 0, 0, sizeof *archiver);
	archiver->cc = calloc(1, sizeof *archiver->cc);

	if (argc == 1) {
		void* const _cc = wrenGetSlotForeign(vm, 1);
		memcpy(archiver->cc, _cc, sizeof *archiver->cc);
	}

	else {
		cc_init(archiver->cc);
	}

	archiver->path = strdup("/usr/bin/ar");
}

static void archiver_del(void* _archiver) {
	archiver_t* const archiver = _archiver;

	if (archiver->path) {
		free(archiver->path);
	}
}

// methods

void archiver_archive(WrenVM* vm) {
	CHECK_ARGC("Archiver.archive", 2, 2)

	archiver_t* const archiver = wrenGetSlotForeign(vm, 0);
	size_t const path_list_len = wrenGetListCount(vm, 1);
	char const* const out = wrenGetSlotString(vm, 2);

	// read list elements & construct exec args

	wrenEnsureSlots(vm, 4); // we just need a single extra slot for each list element

	size_t exec_args_len = 3 + path_list_len + 1 /* NULL sentinel */;
	char** exec_args = calloc(1, exec_args_len * sizeof *exec_args);

	exec_args[0] = strdup(archiver->path);
	exec_args[1] = strdup("-rcs");
	exec_args[2] = strdup(out);

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 3);
		char const* const src_path = wrenGetSlotString(vm, 3);

		// TODO same comment as in 'linker.h'

		char* const abs_path = realpath(src_path, NULL);
		uint64_t const hash = hash_str(abs_path);
		free(abs_path);

		char* path;

		if (asprintf(&path, "bin/%lx.o", hash))
			;

		exec_args[3 + i] = path;
	}

	// wait for all compilation processes to finish

	cc_t* cc = archiver->cc;

	for (size_t i = 0; i < cc->compilation_processes_len; i++) {
		pid_t pid = cc->compilation_processes[i];
		wait_for_process(pid);
	}

	// finally, execute archiver

	pid_t pid = fork();

	if (!pid) {
		if (execv(exec_args[0], exec_args) < 0) {
			LOG_FATAL("execve(\"%s\"): %s", exec_args[0], strerror(errno))
		}

		_exit(EXIT_FAILURE);
	}

	wait_for_process(pid);

	// clean up

	for (size_t i = 0; i < exec_args_len - 1 /* we obv don't want to free the NULL sentinel */; i++) {
		char* const arg = exec_args[i];

		if (!arg) { // shouldn't happen but let's be defensive...
			continue;
		}

		free(arg);
	}

	free(exec_args);
}

// getters

static void archiver_get_path(WrenVM* vm) {
	CHECK_ARGC("Archiver.path", 0, 0)

	archiver_t* archiver = wrenGetSlotForeign(vm, 0);
	wrenSetSlotString(vm, 0, archiver->path);
}

// setters

static void archiver_set_path(WrenVM* vm) {
	CHECK_ARGC("Archiver.path=", 1, 1)

	archiver_t* archiver = wrenGetSlotForeign(vm, 0);

	if (archiver->path) {
		free(archiver->path);
	}

	archiver->path = strdup(wrenGetSlotString(vm, 1));
}

// foreign method binding

static WrenForeignMethodFn archiver_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", archiver_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", archiver_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "archive(_,_)", archiver_archive)

	// unknown

	return unknown_foreign;
}
