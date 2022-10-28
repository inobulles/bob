#define __STDC_WANT_LIB_EXT2__ 1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
	#define _GNU_SOURCE
#endif

#include <umber.h>
#define UMBER_COMPONENT "bob"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wren.h>

#include <sys/stat.h>

#include "util.h"

#include "base/base.h"

#include "classes/cc.h"
#include "classes/file.h"
#include "classes/linker.h"

static WrenForeignMethodFn wren_bind_foreign_method(WrenVM* wm, char const* module, char const* class, bool static_, char const* signature) {
	WrenForeignMethodFn fn = unknown_foreign;

	// classes

	if (!strcmp(class, "CC")) {
		fn = cc_bind_foreign_method(static_, signature);
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

static WrenForeignClassMethods wren_bind_foreign_class(WrenVM* wm, char const* module, char const* class) {
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

int main(int argc, char* argv[]) {
	// XXX for now we're just gonna assume 'bob build' is the only thing being run each time
	// TODO in the future, it'd be nice if this could detect various different scenarios and adapt intelligently, such as not finding a 'build.wren' file but instead finding a 'Makefile'

	// setup wren virtual machine

	WrenConfiguration config;
	wrenInitConfiguration(&config);

	config.writeFn = &wren_write_fn;
	config.errorFn = &wren_error_fn;

	config.bindForeignMethodFn = &wren_bind_foreign_method;
	config.bindForeignClassFn  = &wren_bind_foreign_class;

	WrenVM* vm = wrenNewVM(&config);

	// read build configuration base file

	char const* const module = "main";
	WrenInterpretResult result = wrenInterpret(vm, module, base_src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Build configuration base ran successfully")
	}

	// create bin directory if it doesn't yet exist

	if (mkdir("bin", 0700) < 0 && errno != EEXIST) {
		LOG_FATAL("Couldn't create bin directory: %s", strerror(errno))
		return EXIT_FAILURE;
	}

	// read build configuration file

	FILE* fp = fopen("build.wren", "r");

	if (!fp) {
		LOG_FATAL("Couldn't read 'build.wren'")
		return EXIT_FAILURE;
	}

	fseek(fp, 0, SEEK_END);
	size_t bytes = ftell(fp);
	rewind(fp);

	char* config_src = malloc(bytes);

	if (fread(config_src, 1, bytes, fp))
		;

	config_src[bytes - 1] = 0;

	result = wrenInterpret(vm, module, config_src);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Build configuration ran successfully")
	}

	// clean everything up (not super necessary, I'd put a little more effort in error handling if it was :P)

	wrenFreeVM(vm);

	free(config_src);
	fclose(fp);

	return EXIT_SUCCESS;
}
