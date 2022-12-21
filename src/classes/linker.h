#pragma once

#include "../util.h"

#include "cc.h"
#include <stdlib.h>
#include <unistd.h>

typedef struct {
	cc_t* cc;

	char* path;
	char* archiver_path;
} linker_t;

// constructor/destructor

static void linker_new(WrenVM* vm) {
	CHECK_ARGC("Linker.new", 0, 1)
	bool const has_cc = argc == 1;

	if (has_cc) {
		ASSERT_ARG_TYPE(1, WREN_TYPE_FOREIGN);
	}

	linker_t* const linker = wrenSetSlotNewForeign(vm, 0, 0, sizeof *linker);
	linker->cc = calloc(1, sizeof *linker->cc);

	if (has_cc) {
		void* const _cc = wrenGetSlotForeign(vm, 1);
		memcpy(linker->cc, _cc, sizeof *linker->cc);
	}

	else {
		cc_init(linker->cc);
	}

	linker->path = strdup(linker->cc->path); // use the 'cc' command for linking, not 'ld'
	linker->archiver_path = strdup("ar");
}

static void linker_del(void* _linker) {
	linker_t* const linker = _linker;

	if (linker->path) {
		free(linker->path);
	}
}

// methods

static bool __linker_wait_cc(linker_t* linker) {
	// wait for all compilation processes to finish

	cc_t* const cc = linker->cc;
	bool error = false;

	// if there are none, return successfully straight away

	if (!cc->cc_procs_len) {
		return false;
	}

	progress_t* const progress = progress_new();

	for (size_t i = 0; i < cc->cc_procs_len; i++) {
		cc_proc_t* const cc_proc = &cc->cc_procs[i];
		progress_update(progress, i, cc->cc_procs_len, "Compiling '%s' (%zu of %zu)", cc_proc->name, i + 1, cc->cc_procs_len);

		cc_proc->result = wait_for_process(cc_proc->pid);
		error |= !!cc_proc->result;
	}

	// complete progress

	progress_complete(progress);
	progress_del(progress);

	// print out warning & error messages if there are any

	for (size_t i = 0; i < cc->cc_procs_len; i++) {
		cc_proc_t* const cc_proc = &cc->cc_procs[i];

		// print out stderr of the compilation process
		// we don't only do this on error, because warnings are also printed to stderr

		char* const out = exec_args_read_out(cc_proc->exec_args, PIPE_STDERR);

		if (*out) {
			if (cc_proc->result == EXIT_SUCCESS) {
				LOG_WARN("Compiling '%s' succeeded with warnings:", cc_proc->name)
			}

			else {
				LOG_ERROR("Compiling '%s' failed with errors:", cc_proc->name)
			}

			fprintf(stderr, "%s", out);
		}

		free(out);

		// then, free the 'cc_proc' struct

		free(cc_proc->name);
		exec_args_del(cc_proc->exec_args);
	}

	// print out final message

	if (error) {
		LOG_ERROR("Failed compilation")
	}

	else {
		LOG_SUCCESS("All %zu source files compiled", cc->cc_procs_len)
	}

	// clear the compiler processes list

	free(cc->cc_procs);

	cc->cc_procs = NULL;
	cc->cc_procs_len = 0;

	return error;
}

void linker_link(WrenVM* vm) {
	CHECK_ARGC("Linker.link", 3, 4)
	bool const has_shared = argc == 4;

	ASSERT_ARG_TYPE(1, WREN_TYPE_LIST)
	ASSERT_ARG_TYPE(2, WREN_TYPE_LIST)
	ASSERT_ARG_TYPE(3, WREN_TYPE_STRING)

	if (has_shared) {
		ASSERT_ARG_TYPE(4, WREN_TYPE_BOOL)
	}

	linker_t* const linker = foreign;
	size_t const path_list_len = wrenGetListCount(vm, 1);
	size_t const lib_list_len = wrenGetListCount(vm, 2);
	char const* const out = wrenGetSlotString(vm, 3);
	bool shared = has_shared ? wrenGetSlotBool(vm, 4) : false;

	// construct exec args

	exec_args_t* exec_args = exec_args_new(1, linker->path);
	exec_args_fmt(exec_args, "-L%s", bin_path);

	// start by adding all the C compiler options

	for (size_t i = 0; i < linker->cc->opts_len; i++) {
		exec_args_add(exec_args, linker->cc->opts[i]);
	}

	// then, read list elements

	wrenEnsureSlots(vm, 6); // we just need a single extra slot for each list element

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 5);

		if (wrenGetSlotType(vm, 5) != WREN_TYPE_STRING) {
			LOG_WARN("'Linker.link' list element %zu of argument 1 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const src_path = wrenGetSlotString(vm, 5);

		// TODO maybe we should check if we actually attempted generating this source file in the first place?
		//      because currently, this would still link even if we, say, accidentally deleted a source file between builds
		//      another solution would be to initially stage in an empty directory, and if we want to reuse resources, we explicitly copy from a temporary backup of the old directory (in '/tmp/', whatever)
		//      but that would mean bringing in libcopyfile as a dependency and meeeeeeh

		char* const abs_path = realpath(src_path, NULL);

		if (!abs_path) {
			LOG_WARN("Could not find '%s' - skipping", src_path)
			continue;
		}

		uint64_t const hash = hash_str(abs_path);
		free(abs_path);

		exec_args_fmt(exec_args, "%s/%lx.o", bin_path, hash);
	}

	// libraries

	for (size_t i = 0; i < lib_list_len; i++) {
		wrenGetListElement(vm, 2, i, 5);

		if (wrenGetSlotType(vm, 5) != WREN_TYPE_STRING) {
			LOG_WARN("'Linker.link' list element %zu of argument 2 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const lib = wrenGetSlotString(vm, 5);
		exec_args_fmt(exec_args, "-l%s", lib);
	}

	// linker flags which must come after source files

	if (shared) {
		exec_args_add(exec_args, "-shared");
	}

	exec_args_add(exec_args, "-o");
	exec_args_fmt(exec_args, "%s/%s", bin_path, out);

	// wait for compilation processes and execute linker

	if (__linker_wait_cc(linker)) {
		LOG_FATAL("Error while compiling")
		exit(EXIT_FAILURE);
	}

	execute(exec_args);
	exec_args_del(exec_args);
}

void linker_archive(WrenVM* vm) {
	CHECK_ARGC("Linker.archive", 2, 2)

	ASSERT_ARG_TYPE(1, WREN_TYPE_LIST)
	ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	size_t const path_list_len = wrenGetListCount(vm, 1);
	char const* const out = wrenGetSlotString(vm, 2);

	// read list elements & construct exec args

	wrenEnsureSlots(vm, 4); // we just need a single extra slot for each list element

	exec_args_t* exec_args = exec_args_new(2, linker->archiver_path, "-rcs");
	exec_args_fmt(exec_args, "%s/%s", bin_path, out);

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 3);

		if (wrenGetSlotType(vm, 3) != WREN_TYPE_STRING) {
			LOG_WARN("'Linker.archive' list element %zu of argument 1 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const src_path = wrenGetSlotString(vm, 3);

		// TODO same comment as in 'linker_link'

		char* const abs_path = realpath(src_path, NULL);

		if (!abs_path) {
			LOG_WARN("Could not find '%s' - skipping", src_path)
			continue;
		}

		uint64_t const hash = hash_str(abs_path);
		free(abs_path);

		exec_args_fmt(exec_args, "%s/%lx.o", bin_path, hash);
	}

	// wait for compilation processes and execute archiver

	__linker_wait_cc(linker);

	execute(exec_args);
	exec_args_del(exec_args);
}

// getters

static void linker_get_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path", 0, 0)

	linker_t* const linker = foreign;
	wrenSetSlotString(vm, 0, linker->path);
}

static void linker_get_archiver_path(WrenVM* vm) {
	CHECK_ARGC("Linker.archiver_path", 0, 0)

	linker_t* const linker = foreign;
	wrenSetSlotString(vm, 0, linker->archiver_path);
}

// setters

static void linker_set_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (linker->path) {
		free(linker->path);
	}

	linker->path = strdup(path);
}

static void linker_set_archiver_path(WrenVM* vm) {
	CHECK_ARGC("Linker.archiver_path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (linker->archiver_path) {
		free(linker->archiver_path);
	}

	linker->archiver_path = strdup(path);
}

// foreign method binding

static WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", linker_get_path)
	BIND_FOREIGN_METHOD(false, "archiver_path()", linker_get_archiver_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", linker_set_path)
	BIND_FOREIGN_METHOD(false, "archiver_path=(_)", linker_set_archiver_path)

	// methods

	BIND_FOREIGN_METHOD(false, "link(_,_,_)", linker_link)
	BIND_FOREIGN_METHOD(false, "link(_,_,_,_)", linker_link)
	BIND_FOREIGN_METHOD(false, "archive(_,_)", linker_archive)

	// unknown

	return unknown_foreign;
}
