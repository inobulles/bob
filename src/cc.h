#pragma once

#include "util.h"

typedef struct {
	bool debug;
	char* std;
} cc_t;

// constructor/destructor

static void cc_new(WrenVM* vm) {
	int argc = wrenGetSlotCount(vm) - 1;

	if (argc != 0) {
		LOG_WARN("'CC.new' not passed right number of arguments (%d)", argc)
		return;
	}

	cc_t* cc = calloc(1, sizeof *cc);

	cc->debug = true; // TODO be able to choose between various build types when running the bob command, and CC.debug should default to that obviously
	cc->std = strdup("c99");

	// TODO does it really make sense to have this as a reference?
	//      look at the documentation/example and think about why they do it like they do- should we really be copying them?

	cc_t** cc_ref = wrenSetSlotNewForeign(vm, 0, 0, sizeof *cc_ref);
	*cc_ref = cc;
}

static void cc_del(void* _cc_ref) {
	if (!_cc_ref) {
		return;
	}

	cc_t** cc_ref = _cc_ref;
	cc_t* cc = *cc_ref;

	if (cc->std) {
		free(cc->std);
	}

	free(cc);
	*cc_ref = NULL;
}

// getters

static void cc_get_debug(WrenVM* vm) {
	cc_t** cc_ref = wrenGetSlotForeign(vm, 0);
	cc_t* cc = *cc_ref;

	wrenSetSlotBool(vm, 0, cc->debug);
}

static void cc_get_std(WrenVM* vm) {
	cc_t** cc_ref = wrenGetSlotForeign(vm, 0);
	cc_t* cc = *cc_ref;

	wrenSetSlotString(vm, 0, cc->std);
}

// setters

static void cc_set_debug(WrenVM* vm) {
	// TODO maybe make this a macro, as it's a little redundant as it is

	int argc = wrenGetSlotCount(vm) - 1;

	if (argc != 1) {
		LOG_WARN("'CC.debug=' not passed right number of arguments (%d)", argc)
		return;
	}

	cc_t** cc_ref = wrenGetSlotForeign(vm, 0);
	cc_t* cc = *cc_ref;

	cc->debug = wrenGetSlotBool(vm, 1);
}

static void cc_set_std(WrenVM* vm) {
	int argc = wrenGetSlotCount(vm) - 1;

	if (argc != 1) {
		LOG_WARN("'CC.std=' not passed right number of arguments (%d)", argc)
		return;
	}

	cc_t** cc_ref = wrenGetSlotForeign(vm, 0);
	cc_t* cc = *cc_ref;

	cc->std = strdup(wrenGetSlotString(vm, 1));
}

// methods

static void cc_compile(WrenVM* vm) {
	cc_t** cc_ref = wrenGetSlotForeign(vm, 0);
	cc_t* cc = *cc_ref;

	int argc = wrenGetSlotCount(vm) - 1;

	if (argc != 1) {
		LOG_WARN("'CC.compile' not passed right number of arguments (%d)", argc)
		return;
	}

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
