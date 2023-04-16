#include <util.h>

#include <classes/cc.h>
#include <classes/rustc.h>
#include <classes/linker.h>
#include <classes/package.h>

#include <classes/deps.h>
#include <classes/file.h>
#include <classes/meta.h>
#include <classes/resources.h>

#include <base/base.h>

void wren_unknown_foreign(WrenVM* vm) {
	(void) vm;

	LOG_WARN("Calling unknown foreign function")
}

WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* sig) {
	(void) vm;

	WrenForeignMethodFn fn = wren_unknown_foreign;

	// object classes

	if (!strcmp(class, "CC"))
		fn = cc_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "RustC"))
		fn = rustc_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "Linker"))
		fn = linker_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "Package"))
		fn = package_bind_foreign_method(static_, sig);

	// static classes

	else if (!strcmp(class, "Deps"))
		fn = deps_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "File"))
		fn = file_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "Meta"))
		fn = meta_bind_foreign_method(static_, sig);

	else if (!strcmp(class, "Resources"))
		fn = resources_bind_foreign_method(static_, sig);

	// unknown

	if (fn == wren_unknown_foreign) {
		LOG_WARN("Unknown%s foreign method '%s' in module '%s', class '%s'", static_ ? " static" : "", sig, module, class)
	}

	return fn;
}

WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class) {
	(void) vm;

	WrenForeignClassMethods meth = { 0 };

	if (!strcmp(class, "CC")) {
		meth.allocate = cc_new;
		meth.finalize = cc_del;
	}

	else if (!strcmp(class, "RustC")) {
		meth.allocate = rustc_new;
		meth.finalize = rustc_del;
	}

	else if (!strcmp(class, "Linker")) {
		meth.allocate = linker_new;
		meth.finalize = linker_del;
	}

	else if (!strcmp(class, "Package")) {
		meth.allocate = package_new;
		meth.finalize = package_del;
	}

	else
		LOG_WARN("Unknown foreign class '%s' in module '%s'", class, module)

	return meth;
}

static void write_fn(WrenVM* vm, char const* msg) {
	(void) vm;

	printf("%s", msg);
}

static void error_fn(WrenVM* vm, WrenErrorType type, char const* module, int line, char const* msg) {
	(void) vm;

	if (type == WREN_ERROR_RUNTIME) {
		LOG_ERROR("Wren runtime error: %s", msg)
		return;
	}

	LOG_ERROR("Wren error in module '%s' at line %d: %s", module, line, msg)
}

int wren_setup_vm(state_t* state) {
	// setup wren virtual machine

	wrenInitConfiguration(&state->config);

	state->config.writeFn = &write_fn;
	state->config.errorFn = &error_fn;

	state->config.bindForeignMethodFn = &wren_bind_foreign_method;
	state->config.bindForeignClassFn  = &wren_bind_foreign_class;

	state->vm = wrenNewVM(&state->config);

	// read configuration base file

	char const* const module = "main";
	WrenInterpretResult result = wrenInterpret(state->vm, module, base_src);

	if (result != WREN_RESULT_SUCCESS) {
		LOG_FATAL("Something went wrong executing configuration base")
		return EXIT_FAILURE;
	}

	// read configuration file
	// TODO can the filepointer be closed right after reading?

	state->fp = fopen("build.wren", "r");

	if (!state->fp) {
		LOG_FATAL("Couldn't read 'build.wren'")
		return EXIT_FAILURE;
	}

	size_t const size = file_get_size(state->fp);
	state->src = file_read_str(state->fp, size);

	// run configuration file

	result = wrenInterpret(state->vm, module, state->src);

	if (result != WREN_RESULT_SUCCESS) {
		LOG_FATAL("Something wrong executing build configuration")
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void wren_clean_vm(state_t* state) {
	if (state->vm)
		wrenFreeVM(state->vm);

	if (state->src)
		free(state->src);

	if (state->fp)
		fclose(state->fp);
}

int wren_call(state_t* state, char const* class, char const* sig, char const** str_ref) {
	// check class exists

	if (!wrenHasVariable(state->vm, "main", class)) {
		LOG_ERROR("No '%s' class", class)
		return EXIT_FAILURE;
	}

	// get class handle

	wrenEnsureSlots(state->vm, 1);
	wrenGetVariable(state->vm, "main", class, 0);
	WrenHandle* const class_handle = wrenGetSlotHandle(state->vm, 0);

	// get method handle

	WrenHandle* const method_handle = wrenMakeCallHandle(state->vm, sig);
	wrenReleaseHandle(state->vm, class_handle);

	// actually call function

	WrenInterpretResult const rv = wrenCall(state->vm, method_handle);
	wrenReleaseHandle(state->vm, method_handle);

	// error checking & return value

	if (rv != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running")
		return EXIT_FAILURE;
	}

	if (wrenGetSlotType(state->vm, 0) == WREN_TYPE_NUM) {
		double const _rv = wrenGetSlotDouble(state->vm, 0);
		return _rv;
	}

	else if (wrenGetSlotType(state->vm, 0) == WREN_TYPE_BOOL)
		return !wrenGetSlotBool(state->vm, 0);

	else if (str_ref && wrenGetSlotType(state->vm, 0) == WREN_TYPE_STRING) {
		char const* const _str = wrenGetSlotString(state->vm, 0);
		*str_ref = _str;

		return _str ? EXIT_SUCCESS : EXIT_FAILURE;
	}


	LOG_WARN("Expected number or boolean as a return value")
	return EXIT_FAILURE;
}

int wren_call_args(state_t* state, char const* class, char const* sig, int argc, char** argv) {
	// pass argument list as first argument

	wrenEnsureSlots(state->vm, 3);
	wrenSetSlotNewList(state->vm, 1);

	while (argc --> 0) {
		wrenSetSlotString(state->vm, 2, *argv++);
		wrenInsertInList(state->vm, 1, -1, 2);
	}

	// actually call

	return wren_call(state, class, sig, NULL);
}

int wren_read_map(state_t* state, char const* name, WrenHandle** map_handle_ref, size_t* keys_len_ref) {
	int rv = EXIT_SUCCESS;

	// read installation map

	if (!wrenHasVariable(state->vm, "main", name)) {
		LOG_ERROR("No '%s' map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	wrenEnsureSlots(state->vm, 1); // for the receiver (starts off as the map, ends up being the keys list)
	wrenGetVariable(state->vm, "main", name, 0);

	if (wrenGetSlotType(state->vm, 0) != WREN_TYPE_MAP) {
		LOG_ERROR("'%s' variable is not a map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	size_t const map_len = wrenGetMapCount(state->vm, 0);

	// keep handle to installation map

	if (map_handle_ref)
		*map_handle_ref = wrenGetSlotHandle(state->vm, 0);

	// run the 'keys' method on the map

	WrenHandle* const keys_handle = wrenMakeCallHandle(state->vm, "keys");
	WrenInterpretResult const keys_result = wrenCall(state->vm, keys_handle); // no need to set receiver - it's already in slot 0
	wrenReleaseHandle(state->vm, keys_handle);

	if (keys_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'keys' method on the '%s' map", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	// run the 'toList' method on the 'MapKeySequence' object

	WrenHandle* const to_list_handle = wrenMakeCallHandle(state->vm, "toList");
	WrenInterpretResult const to_list_result = wrenCall(state->vm, to_list_handle);
	wrenReleaseHandle(state->vm, to_list_handle);

	if (to_list_result != WREN_RESULT_SUCCESS) {
		LOG_ERROR("Something went wrong running the 'toList' method on the '%s' map keys' 'MapKeySequence'", name)

		rv = EXIT_FAILURE;
		goto err;
	}

	// small sanity check - is the converted keys list as big as the map?

	size_t const keys_len = wrenGetListCount(state->vm, 0);

	if (map_len != keys_len) {
		LOG_ERROR("'%s' map is not the same size as converted keys list (%zu vs %zu)", name, map_len, keys_len)

		rv = EXIT_FAILURE;
		goto err;
	}

	if (keys_len_ref)
		*keys_len_ref = keys_len;

err:

	return rv;
}
