#include <util.h>

#include <classes/resources.h>

// foreign method binding

WrenForeignMethodFn resources_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "install(_)", resources_install)

	// unknown

	return wren_unknown_foreign;
}

// methods

void resources_install(WrenVM* vm) {
	CHECK_ARGC("Resources.install", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	char const* const path = wrenGetSlotString(vm, 1);

	// copy file to output directory

	char const* const base = basename(path);

	char* CLEANUP_STR dest = NULL;
	if (asprintf(&dest, "%s/%s", bin_path, base)) {}

	char* const CLEANUP_STR err = copy_recursive(path, dest);

	if (err != NULL) {
		LOG_ERROR("Failed to copy resource '%s' to '%s': %s", path, dest, err)
	}
}
