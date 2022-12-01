#pragma once

#include "base/base.h"

#include "classes/cc.h"
#include "classes/file.h"
#include "classes/linker.h"

static WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* signature) {
	WrenForeignMethodFn fn = unknown_foreign;

	// classes

	if (!strcmp(class, "CC")) {
		fn = cc_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "Deps")) {
		fn = deps_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "File")) {
		fn = file_bind_foreign_method(static_, signature);
	}

	else if (!strcmp(class, "Linker")) {
		fn = linker_bind_foreign_method(static_, signature);
	}

	// unknown

	if (fn == unknown_foreign) {
		LOG_WARN("Unknown%s foreign method '%s' in module '%s', class '%s'", static_ ? " static" : "", signature, module, class)
	}

	return fn;
}

static WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class) {
	WrenForeignClassMethods meth = { NULL };

	if (!strcmp(class, "CC")) {
		meth.allocate = cc_new;
		meth.finalize = cc_del;
	}

	else if (!strcmp(class, "Linker")) {
		meth.allocate = linker_new;
		meth.finalize = linker_del;
	}

	else {
		LOG_WARN("Unknown foreign class '%s' in module '%s'", class, module)
	}

	return meth;
}

typedef struct {
	WrenConfiguration config;
	WrenVM* vm;

	// configuration file

	FILE* fp;
	char* src;
} state_t;

static int wren_setup_vm(state_t* state) {
	// setup wren virtual machine

	wrenInitConfiguration(&state->config);

	state->config.writeFn = &wren_write_fn;
	state->config.errorFn = &wren_error_fn;

	state->config.bindForeignMethodFn = &wren_bind_foreign_method;
	state->config.bindForeignClassFn  = &wren_bind_foreign_class;

	state->vm = wrenNewVM(&state->config);

	// read configuration base file

	char const* const module = "main";
	WrenInterpretResult result = wrenInterpret(state->vm, module, base_src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Configuration base ran successfully")
	}

	// read configuration file

	state->fp = fopen("build.wren", "r");

	if (!state->fp) {
		LOG_FATAL("Couldn't read 'build.wren'")
		return EXIT_FAILURE;
	}

	fseek(state->fp, 0, SEEK_END);
	size_t const bytes = ftell(state->fp);
	rewind(state->fp);

	state->src = malloc(bytes);

	if (fread(state->src, 1, bytes, state->fp))
		;

	state->src[bytes - 1] = 0;

	// run configuration file

	result = wrenInterpret(state->vm, module, state->src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Build configuration ran successfully")
	}

	return EXIT_SUCCESS;
}

static int wren_call(state_t* state, char const* name) {
	if (!wrenHasVariable(state->vm, "main", name)) {
		LOG_ERROR("No run function")
		return EXIT_FAILURE;
	}

	wrenEnsureSlots(state->vm, 1);
	wrenGetVariable(state->vm, "main", "run", 0);
	WrenHandle* handle = wrenGetSlotHandle(state->vm, 0);

	WrenInterpretResult rv = wrenCall(state->vm, handle);

	printf("rv = %d\n", rv);

	wrenReleaseHandle(state->vm, handle);
	return EXIT_SUCCESS;
}

static void wren_clean_vm(state_t* state) {
	// clean everything up (not super necessary, I'd put a little more effort in error handling if it was :P)

	wrenFreeVM(state->vm);

	free(state->src);
	fclose(state->fp);
}

static int do_build(void) {
	state_t state;
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		return rv;
	}

	wren_clean_vm(&state);
	return EXIT_SUCCESS;
}

static int do_run(void) {
	state_t state;
	int rv = wren_setup_vm(&state);

	if (rv != EXIT_SUCCESS) {
		goto error;
	}

	// call the run function

	rv = wren_call(&state, "run");

	if (rv != EXIT_SUCCESS) {
		goto error;
	}

error:

	wren_clean_vm(&state);
	return rv;
}
