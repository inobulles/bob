#pragma once

#include <util.h>

// methods

static void resources_install(WrenVM* vm) {
	CHECK_ARGC("Resources.install", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	char const* const path = wrenGetSlotString(vm, 1);

	// copy file to output directory

	copy_recursive(path, bin_path);
}

// foreign method binding

static WrenForeignMethodFn resources_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "install(_)", resources_install)

	// unknown

	return unknown_foreign;
}
