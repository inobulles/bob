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

	linker_t* linker = wrenGetSlotForeign(vm, 0);

	WrenHandle* path_list_handle = wrenGetSlotHandle(vm, 1);

	wrenReleaseHandle(vm, path_list_handle);
}

// foreign method binding

static WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature) {
	// methods
	
	BIND_FOREIGN_METHOD(false, "link(_,_)", linker_link)

	// unknown

	return unknown_foreign;
}
