#pragma once

#include "../util.h"

typedef struct {
	bool debug;
	char* std;
} cc_t;

// helpers

static void cc_init(cc_t* cc) {
	cc->debug = true; // TODO be able to choose between various build types when running the bob command, and CC.debug should default to that obviously
	cc->std = strdup("c99");
}

// constructor/destructor

static void cc_new(WrenVM* vm) {
	CHECK_ARGC("CC.new", 0, 0)

	cc_t* cc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *cc);
	bzero(cc, sizeof *cc);

	cc_init(cc);
}

static void cc_del(void* _cc) {
	cc_t* cc = _cc;

	if (cc->std) {
		free(cc->std);
	}
}

// getters

static void cc_get_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug", 0, 0)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	wrenSetSlotBool(vm, 0, cc->debug);
}

static void cc_get_std(WrenVM* vm) {
	CHECK_ARGC("CC.std", 0, 0)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	wrenSetSlotString(vm, 0, cc->std);
}

// setters

static void cc_set_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug=", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	cc->debug = wrenGetSlotBool(vm, 1);
}

static void cc_set_std(WrenVM* vm) {
	CHECK_ARGC("CC.std=", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);
	cc->std = strdup(wrenGetSlotString(vm, 1));
}

// methods

static void cc_compile(WrenVM* vm) {
	CHECK_ARGC("CC.compile", 1, 1)

	cc_t* cc = wrenGetSlotForeign(vm, 0);

	char const* _path = wrenGetSlotString(vm, 1);
	char* path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
	}

	uint64_t hash = hash_str(path);

	char* obj_path;

	if (asprintf(&obj_path, "bin/%lx.o", hash))
		;

	LOG_INFO("'%s' -> '%s'", _path, obj_path)

	// TODO break here if object is more recent than source
	//      what happens if options change in the meantime though?

	char* cmd;

	if (asprintf(&cmd, "cc %s -std=%s -I/usr/local/include -c %s -o %s", cc->debug ? "-g" : "", cc->std, path, obj_path))
		;

	system(cmd);

	free(cmd);
	free(obj_path);
	free(path);
}

// foreign method binding

static WrenForeignMethodFn cc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "debug()", cc_get_debug)
	BIND_FOREIGN_METHOD(false, "std()", cc_get_std)

	// setters

	BIND_FOREIGN_METHOD(false, "debug=(_)", cc_set_debug)
	BIND_FOREIGN_METHOD(false, "std=(_)", cc_set_std)

	// methods

	BIND_FOREIGN_METHOD(false, "compile(_)", cc_compile)

	// unknown

	return unknown_foreign;
}
