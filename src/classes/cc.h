#pragma once

#include "../util.h"

#include <errno.h>
#include <unistd.h>

typedef struct {
	bool debug;
	char* path;

	char** opts;
	size_t opts_len;

	pid_t* compilation_processes;
	size_t compilation_processes_len;
} cc_t;

// helpers

static void cc_init(cc_t* cc) {
	cc->debug = true; // TODO be able to choose between various build types when running the bob command, and CC.debug should default to that obviously
	cc->path = strdup("/usr/bin/cc");

	cc->opts = NULL;
	cc->opts_len = 0;

	cc->compilation_processes = NULL;
	cc->compilation_processes_len = 0;
}

static inline void cc_internal_add_opt(cc_t* const cc, char const* const opt) {
	cc->opts = realloc(cc->opts, ++cc->opts_len * sizeof *cc->opts);
	cc->opts[cc->opts_len - 1] = strdup(opt);
}

// constructor/destructor

static void cc_new(WrenVM* vm) {
	CHECK_ARGC("CC.new", 0, 0)

	cc_t* cc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *cc);
	cc_init(cc);
}

static void cc_del(void* _cc) {
	cc_t* cc = _cc;

	if (cc->path) {
		free(cc->path);
	}

	for (size_t i = 0; i < cc->opts_len; i++) {
		char* const opt = cc->opts[i];

		if (!opt) {
			continue;
		}

		free(opt);
	}

	if (cc->opts) {
		free(cc->opts);
	}
}

// getters

static void cc_get_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug", 0, 0)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, cc->debug);
}

static void cc_get_path(WrenVM* vm) {
	CHECK_ARGC("CC.path", 0, 0)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	wrenSetSlotString(vm, 0, cc->path);
}

// setters

static void cc_set_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug=", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	cc->debug = wrenGetSlotBool(vm, 1);
}

static void cc_set_path(WrenVM* vm) {
	CHECK_ARGC("CC.path=", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);

	if (cc->path) {
		free(cc->path);
	}

	cc->path = strdup(wrenGetSlotString(vm, 1));
}

// methods

static void cc_add_opt(WrenVM* vm) {
	CHECK_ARGC("CC.add_opt", 1, 1)

	cc_t* const cc = wrenGetSlotForeign(vm, 0);
	char const* const opt = wrenGetSlotString(vm, 1);

	cc_internal_add_opt(cc, opt);
}

static void cc_compile(WrenVM* vm) {
	CHECK_ARGC("CC.compile", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);

	char const* const _path = wrenGetSlotString(vm, 1);
	char* const path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
	}

	uint64_t const hash = hash_str(path);

	char* obj_path;

	if (asprintf(&obj_path, "%s/%lx.o", bin_path, hash))
		;

	LOG_INFO("'%s' -> '%s'", _path, obj_path)

	// TODO break here if object is more recent than source
	//      what happens if options change in the meantime though?
	//      maybe I could hash the options list (XOR each option's hash together) and store that in 'bin/'?

	// construct exec args

	size_t exec_args_len = 6 + cc->opts_len + 1 /* NULL sentinel */;
	char** exec_args = calloc(exec_args_len, sizeof *exec_args);

	exec_args[0] = cc->path;
	exec_args[1] = cc->debug ? "-g" : "";

	exec_args[2] = "-c";
	exec_args[3] = path;

	exec_args[4] = "-o";
	exec_args[5] = obj_path;

	// copy options into exec args

	for (size_t i = 0; i < cc->opts_len; i++) {
		exec_args[6 + i] = cc->opts[i];
	}

	// finally, actually compile

	pid_t pid = fork();

	if (!pid) {
		if (execv(exec_args[0], exec_args) < 0) {
			LOG_FATAL("execve(\"%s\"): %s", exec_args[0], strerror(errno))
		}

		_exit(EXIT_FAILURE);
	}

	// add pid to list of processes

	cc->compilation_processes = realloc(cc->compilation_processes, ++cc->compilation_processes_len * sizeof *cc->compilation_processes);
	cc->compilation_processes[cc->compilation_processes_len - 1] = pid;

	// clean up
	// we don't need to free the contents of 'exec_args'!

	free(exec_args);
	free(obj_path);
	free(path);
}

// foreign method binding

static WrenForeignMethodFn cc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "debug()", cc_get_debug)
	BIND_FOREIGN_METHOD(false, "path()", cc_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "debug=(_)", cc_set_debug)
	BIND_FOREIGN_METHOD(false, "path=(_)", cc_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "add_opt(_)", cc_add_opt)
	BIND_FOREIGN_METHOD(false, "compile(_)", cc_compile)

	// unknown

	return unknown_foreign;
}
