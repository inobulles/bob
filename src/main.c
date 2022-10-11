#include <umber.h>
#define UMBER_COMPONENT "bob"

#include <wren.h>
#include <stdio.h>
#include <string.h>

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

static void cc_compile(WrenVM* vm) {
	int slot_len = wrenGetSlotCount(vm);

	if (slot_len < 1) {
		LOG_WARN("'CC.compile' not passed enough arguments (%d)", slot_len - 1)
		return;
	}

	char const* path = wrenGetSlotString(vm, 1);
	LOG_INFO("Compile '%s'", path)
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

int main(char* argv[], int argc) {
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
