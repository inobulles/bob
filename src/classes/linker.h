#pragma once

#include "../util.h"

#include "cc.h"

typedef struct {
	cc_t* cc;
} linker_t;

// constructor/desctructor

static void linker_new(WrenVM* vm) {
	CHECK_ARGC("Linker.new", 0, 1)

	linker_t* linker = wrenSetSlotNewForeign(vm, 0, 0, sizeof *linker);
	bzero(linker, sizeof *linker); // XXX does 'wrenSetSlotNewForeign' automatically zero things out (guaranteed)?

	linker->cc = calloc(1, sizeof *linker->cc);

	if (argc == 2) {
		void* const _cc = wrenGetSlotForeign(vm, 0);
		memcpy(linker->cc, _cc, sizeof *linker->cc);
	}

	else {
		cc_init(linker->cc);
	}
}

static void linker_del(void* _linker) {
	linker_t* linker = _linker;

	if (linker->cc) {
		cc_del(linker->cc);
		linker->cc = NULL;
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

	size_t exec_args_len = 4;
	char** exec_args = malloc(exec_args_len * sizeof *exec_args);

	exec_args[0] = strdup("cc");
	exec_args[1] = strdup("cc");
	exec_args[2] = strdup("-o");
	exec_args[3] = strdup(out);

	for (size_t i = 0; i < path_list_len; i++) {
		wrenGetListElement(vm, 1, i, 3);
		char const* const src_path = wrenGetSlotString(vm, 3);

		uint64_t const hash = hash_str(src_path);

		char* path;

		if (asprintf(&path, "bin/%lx.o", hash))
			;

		exec_args = realloc(exec_args, ++exec_args_len * sizeof *exec_args);
		exec_args[exec_args_len - 1] = path;
	}

	// don't forget NULL sentinel!

	exec_args = realloc(exec_args, ++exec_args_len * sizeof *exec_args);
	exec_args[exec_args_len - 1] = NULL;

	// TODO finally, execute linker

	// clean up

	for (size_t i = 0; i < exec_args_len - 1 /* we obv don't want to free the NULL sentinel */; i++) {
		LOG_INFO("= %s", exec_args[i])
		free(exec_args[i]);
	}

	free(exec_args);
}

// foreign method binding

static WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature) {
	// methods
	
	BIND_FOREIGN_METHOD(false, "link(_,_)", linker_link)

	// unknown

	return unknown_foreign;
}
