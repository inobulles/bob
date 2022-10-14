#pragma once

#include "../util.h"

#include "cc.h"

#include <unistd.h>

typedef struct {
	cc_t* cc;
	char* path;
} linker_t;

// constructor/desctructor

static void linker_new(WrenVM* vm) {
	CHECK_ARGC("Linker.new", 0, 1)

	linker_t* const linker = wrenSetSlotNewForeign(vm, 0, 0, sizeof *linker);
	bzero(linker, sizeof *linker); // XXX does 'wrenSetSlotNewForeign' automatically zero things out (guaranteed)?

	linker->cc = calloc(1, sizeof *linker->cc);

	if (argc == 1) {
		void* const _cc = wrenGetSlotForeign(vm, 1);
		memcpy(linker->cc, _cc, sizeof *linker->cc);
	}

	else {
		cc_init(linker->cc);
	}

	linker->path = strdup(linker->cc->path);
}

static void linker_del(void* _linker) {
	linker_t* const linker = _linker;

	if (linker->path) {
		free(linker->path);
	}
}

// methods

void linker_link(WrenVM* vm) {
	CHECK_ARGC("Linker.link", 2, 2)

	linker_t* const linker = wrenGetSlotForeign(vm, 0);
	size_t const path_list_len = wrenGetListCount(vm, 1);
	char const* const out = wrenGetSlotString(vm, 2);

	// read list elements & construct exec args

	wrenEnsureSlots(vm, 4); // we just need a single extra slot for each list element

	size_t exec_args_len = 1 + path_list_len + 4 + 1 /* NULL sentinel */;
	char** exec_args = calloc(1, exec_args_len * sizeof *exec_args);

	exec_args[0] = strdup(linker->path);

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 3);
		char const* const src_path = wrenGetSlotString(vm, 3);

		// TODO maybe we should check if we actually attempted generating this source file in the first place?
		//      because currently, this would still link even if we, say, accidentally deleted a source file between builds
		//      another solution would be to initially stage in an empty directory, and if we want to reuse resources, we explicitly copy from a temporary backup of the old directory (in '/tmp/', whatever)

		char* const abs_path = realpath(src_path, NULL);
		uint64_t const hash = hash_str(abs_path);
		free(abs_path);

		char* path;

		if (asprintf(&path, "bin/%lx.o", hash))
			;

		exec_args[1 + i] = path;
	}

	// linker flags which must come after source files

	exec_args[1 + path_list_len + 0] = strdup("-lm");
	exec_args[1 + path_list_len + 1] = strdup("-lumber");
	exec_args[1 + path_list_len + 2] = strdup("-o");
	exec_args[1 + path_list_len + 3] = strdup(out);

	// wait for all compilation processes to finish

	cc_t* cc = linker->cc;

	for (size_t i = 0; i < cc->compilation_processes_len; i++) {
		pid_t pid = cc->compilation_processes[i];
		wait_for_process(pid);
	}

	// print out all exec args verbosely

	for (size_t i = 0; i < exec_args_len - 1; i++) {
		LOG_VERBOSE("Linker argument: %s", exec_args[i])
	}

	// finally, execute linker

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

static void linker_get_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path", 0, 0)

	linker_t* linker = wrenGetSlotForeign(vm, 0);
	wrenSetSlotString(vm, 0, linker->path);
}

// setters

static void linker_set_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path=", 1, 1)

	linker_t* linker = wrenGetSlotForeign(vm, 0);

	if (linker->path) {
		free(linker->path);
	}

	linker->path = strdup(wrenGetSlotString(vm, 1));
}

// foreign method binding

static WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", linker_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", linker_set_path)

	// methods
	
	BIND_FOREIGN_METHOD(false, "link(_,_)", linker_link)

	// unknown

	return unknown_foreign;
}
