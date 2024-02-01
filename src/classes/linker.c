// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <util.h>
#include <classes/linker.h>

typedef struct {
	char* path;
	char* archiver_path;

	opts_t opts;
} linker_t;

// foreign method binding

WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", linker_get_path)
	BIND_FOREIGN_METHOD(false, "archiver_path()", linker_get_archiver_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", linker_set_path)
	BIND_FOREIGN_METHOD(false, "archiver_path=(_)", linker_set_archiver_path)

	// methods

	BIND_FOREIGN_METHOD(false, "add_opt(_)", linker_add_opt)
	BIND_FOREIGN_METHOD(false, "add_lib(_)", linker_add_lib)

	BIND_FOREIGN_METHOD(false, "link(_,_,_)", linker_link)
	BIND_FOREIGN_METHOD(false, "link(_,_,_,_)", linker_link)
	BIND_FOREIGN_METHOD(false, "archive(_,_)", linker_archive)

	// unknown

	return wren_unknown_foreign;
}

// helpers

static int add_lib(linker_t* linker, char const* lib) {
	exec_args_t* exec_args = exec_args_new(4, "pkg-config", "--libs", "--cflags", lib);
	exec_args_save_out(exec_args, PIPE_STDOUT);

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	char* const orig_opts = exec_args_read_out(exec_args, PIPE_STDOUT);
	char* opts = orig_opts;

	char* opt;

	while ((opt = strsep(&opts, " "))) {
		if (*opt == '\n') {
			continue;
		}

		char* const end = opt + strlen(opt) - 1;

		if (*end == '\n') {
			*end = '\0';
		}

		opts_add(&linker->opts, opt);
	}

	free(orig_opts);

err:

	exec_args_del(exec_args);
	return rv;
}

// constructor/destructor

void linker_new(WrenVM* vm) {
	CHECK_ARGC("Linker.new", 0, 0)

	linker_t* const linker = wrenSetSlotNewForeign(vm, 0, 0, sizeof *linker);

	// use the 'cc' command for linking, not 'ld'

	linker->path = strdup("cc");
	linker->archiver_path = strdup("ar");
}

void linker_del(void* _linker) {
	linker_t* const linker = _linker;

	if (linker->path) {
		free(linker->path);
	}

	if (linker->archiver_path) {
		free(linker->archiver_path);
	}
}

// getters

void linker_get_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path", 0, 0)

	linker_t* const linker = foreign;
	wrenSetSlotString(vm, 0, linker->path);
}

void linker_get_archiver_path(WrenVM* vm) {
	CHECK_ARGC("Linker.archiver_path", 0, 0)

	linker_t* const linker = foreign;
	wrenSetSlotString(vm, 0, linker->archiver_path);
}

// setters

void linker_set_path(WrenVM* vm) {
	CHECK_ARGC("Linker.path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (linker->path) {
		free(linker->path);
	}

	linker->path = strdup(path);
}

void linker_set_archiver_path(WrenVM* vm) {
	CHECK_ARGC("Linker.archiver_path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (linker->archiver_path) {
		free(linker->archiver_path);
	}

	linker->archiver_path = strdup(path);
}

// methods
// TODO make this add lib (haha adlib skrrrrrt) stuff like way better

void linker_add_lib(WrenVM* vm) {
	CHECK_ARGC("Linker.add_lib", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const lib = wrenGetSlotString(vm, 1);

	int rv = add_lib(linker, lib);

	if (rv) {
		LOG_WARN("'Linker.add_lib' failed to add '%s' (error code is %d)", lib, rv);
	}
}

void linker_add_opt(WrenVM* vm) {
	CHECK_ARGC("Linker.add_opt", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	linker_t* const linker = foreign;
	char const* const opt = wrenGetSlotString(vm, 1);

	opts_add(&linker->opts, opt);
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
	bool const shared = has_shared ? wrenGetSlotBool(vm, 4) : false;

	// extra argument checks

	if (!path_list_len) {
		LOG_WARN("'%s' passed an empty list of paths", __fn_name)
		return;
	}

	// wait for compilation tasks

	if (wait_for_tasks(TASK_KIND_COMPILE)) {
		LOG_ERROR("Error while compiling")
		return;
	}

	// create output path

	char* CLEANUP_STR out_path = NULL;
	if (asprintf(&out_path, "%s/%s", bin_path, out)) {}

	bool changed = false;

	// construct exec args
	// we pass '-u__native_entry' to the linker so that it preserves what's necessary for the '__native_entry' symbol
	// this is only necessary for AQUA, but there's no harm leaving it

	exec_args_t* const exec_args = exec_args_new(2, linker->path, "-Wl,--gc-sections,-u__native_entry");
	exec_args_add_opts(exec_args, &linker->opts);
	exec_args_fmt(exec_args, "-L%s", bin_path);
	exec_args_add(exec_args, "-L/usr/local/lib");

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

		char* const CLEANUP_STR abs_path = realpath(src_path, NULL);

		if (!abs_path) {
			LOG_WARN("Could not find '%s' - skipping", src_path)
			continue;
		}

		uint64_t const hash = hash_str(abs_path);
		exec_args_fmt(exec_args, "%s/%lx.o", bin_path, hash);

		// check if last file we added to 'exec_args' is newer than output file
		// we go ahead with linking so long as at least one is newer
		// if we already know this to be the case, we don't need to check

		if (changed) {
			continue;
		}

		char* const obj_path = exec_args->args[exec_args->len - 2];
		changed |= !be_frugal(obj_path, out_path);
	}

	// if nothing has changed, just give up

	if (!changed) {
		exec_args_del(exec_args);
		return;
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
	exec_args_add(exec_args, out_path);

	// execute linker

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

	// extra argument checks

	if (!path_list_len) {
		LOG_WARN("'%s' passed an empty list of paths", __fn_name)
		return;
	}

	// wait for compilation tasks

	if (wait_for_tasks(TASK_KIND_COMPILE)) {
		LOG_ERROR("Error while compiling")
		return;
	}

	// create output path

	char* CLEANUP_STR out_path = NULL;
	if (asprintf(&out_path, "%s/%s", bin_path, out)) {}

	bool changed = false;

	// read list elements & construct exec args

	wrenEnsureSlots(vm, 4); // we just need a single extra slot for each list element

	exec_args_t* const exec_args = exec_args_new(2, linker->archiver_path, "-rcs");
	exec_args_add_opts(exec_args, &linker->opts);
	exec_args_add(exec_args, out_path);

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 3);

		if (wrenGetSlotType(vm, 3) != WREN_TYPE_STRING) {
			LOG_WARN("'Linker.archive' list element %zu of argument 1 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const src_path = wrenGetSlotString(vm, 3);

		// TODO same comment as in 'linker_link'

		char* const CLEANUP_STR abs_path = realpath(src_path, NULL);

		if (!abs_path) {
			LOG_WARN("Could not find '%s' - skipping", src_path)
			continue;
		}

		uint64_t const hash = hash_str(abs_path);
		exec_args_fmt(exec_args, "%s/%lx.o", bin_path, hash);

		// check if last file we added to 'exec_args' is newer than output file
		// we go ahead with archiving so long as at least one is newer
		// if we already know this to be the case, we don't need to check

		if (changed) {
			continue;
		}

		char* const obj_path = exec_args->args[exec_args->len - 2];
		changed |= !be_frugal(obj_path, out_path);
	}

	// if nothing has changed, just give up

	if (!changed) {
		exec_args_del(exec_args);
		return;
	}

	// execute archiver

	execute(exec_args);
	exec_args_del(exec_args);
}

