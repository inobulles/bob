// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <util.h>

#include <classes/resources.h>

#include <libgen.h>

// foreign method binding

WrenForeignMethodFn resources_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "install(_)", resources_install)
	BIND_FOREIGN_METHOD(true, "install(_,_)", resources_install)

	// unknown

	return wren_unknown_foreign;
}

// methods

void resources_install(WrenVM* vm) {
	CHECK_ARGC("Resources.install", 1, 2)
	bool const has_dest = argc == 2;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	if (has_dest) {
		ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)
	}

	char const* const path = wrenGetSlotString(vm, 1);

	char* const CLEANUP_STR writable_path = strdup(path);
	char const* const dest = has_dest ? wrenGetSlotString(vm, 2) : basename(writable_path);

	char* CLEANUP_STR dest_path = NULL;
	if (asprintf(&dest_path, "%s/%s", bin_path, dest)) {}

	if (dest) {
		// make parent directory if it doesn't yet exist

		char* CLEANUP_STR parent_backing = strdup(dest_path);
		char const* const parent = dirname(parent_backing);

		if (*parent != '.') {
			mkdir_recursive(parent);
		}

	}

	// copy file to output directory

	char* const CLEANUP_STR err = copy_recursive(path, dest_path);

	if (err != NULL) {
		LOG_ERROR("Failed to copy resource '%s' to '%s': %s", path, dest, err)
	}
}
