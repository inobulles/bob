#include <util.h>

void wren_write_fn(WrenVM* vm, char const* msg) {
	(void) vm;

	printf("%s", msg);
}

void wren_error_fn(WrenVM* vm, WrenErrorType type, char const* module, int line, char const* msg) {
	(void) vm;

	if (type == WREN_ERROR_RUNTIME) {
		LOG_ERROR("Wren runtime error: %s", msg)
		return;
	}

	LOG_ERROR("Wren error in module '%s' at line %d: %s", module, line, msg)
}

void unknown_foreign(WrenVM* vm) {
	(void) vm;

	LOG_WARN("Calling unknown foreign function")
}
