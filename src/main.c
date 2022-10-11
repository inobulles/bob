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

static void wren_write_fn(WrenVM* vm, char const* msg) {
	printf("%s", msg);
}

static void wren_error_fn(WrenVM* vm, WrenErrorType type, char const* module, int line, char const* msg) {
	if (type == WREN_ERROR_RUNTIME) {
		LOG_ERROR("Wren runtime error: %s", msg)
		return;
	}

	LOG_ERROR("Wren error in module '%s' at line %d: %s", module, line, msg)
}

static void unknown_foreign(WrenVM* vm) {
	LOG_WARN("Calling unknown foreign function")
}

static uint64_t hash_str(char const* str) { // djb2 algorithm
	uint64_t hash = 5381;
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++) {
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]
	}

	return hash;
}

static void cc_compile(WrenVM* vm) {
	int slot_len = wrenGetSlotCount(vm);

	if (slot_len < 1) {
		LOG_WARN("'CC.compile' not passed enough arguments (%d)", slot_len - 1)
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

	if (asprintf(&cmd, "cc -c %s -o %s", path, obj_path))
		;

	system(cmd);

	free(cmd);
	free(obj_path);
	free(path);
}

static WrenForeignMethodFn wren_bind_foreign_method_fn(WrenVM* wm, char const* module, char const* class, bool is_static, char const* signature) {
	if (!strcmp(class, "CC")) {
		if (!strcmp(signature, "compile(_)")) {
			return cc_compile;
		}
	}

	LOG_WARN("Unknown %sforeign method '%s' in module '%s', class '%s'", is_static ? "static " : "", signature, module, class)
	return unknown_foreign;
}

int main(int argc, char* argv[]) {
	// XXX for now we're just gonna assume 'bob build' is the only thing being run each time
	// read build configuration file
	// TODO in the future, it'd be nice if this could detect various different scenarios and adapt intelligently, such as not finding a 'build.wren' file but instead finding a 'Makefile'
	
	FILE* fp = fopen("build.wren", "r");

	if (!fp) {
		LOG_FATAL("Couldn't read 'build.wren'!")
		return EXIT_FAILURE;
	}

	fseek(fp, 0, SEEK_END);
	size_t bytes = ftell(fp);
	rewind(fp);

	char* config = malloc(bytes);

	if (fread(config, 1, bytes, fp))
		;

	WrenConfiguration wren_config;
	wrenInitConfiguration(&wren_config);

	wren_config.writeFn = &wren_write_fn;
	wren_config.errorFn = &wren_error_fn;
	wren_config.bindForeignMethodFn = &wren_bind_foreign_method_fn;

	WrenVM* vm = wrenNewVM(&wren_config);

	char const* module = "main";

	WrenInterpretResult result = wrenInterpret(vm, module, config);

	if (result == WREN_RESULT_SUCCESS) {
		LOG_SUCCESS("Build configuration ran successfully")
	}

	// clean everything up (not super necessary, I'd put a little more effort in error handling if it was :P)

	wrenFreeVM(vm);

	free(config);
	fclose(fp);

	return EXIT_SUCCESS;
}
