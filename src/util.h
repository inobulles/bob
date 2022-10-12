#pragma once

// useful macros

#define BIND_FOREIGN_METHOD(_static_, _signature, fn) \
	if (static_ == (_static_) && !strcmp(signature, (_signature))) { \
		return (fn); \
	}

#define CHECK_ARGC(fn_name, _argc) \
	int argc = wrenGetSlotCount(vm) - 1; \
	\
	if (argc != (_argc)) { \
		LOG_WARN("'" fn_name "' not passed right number of arguments (got %d, expected %d)", argc, (_argc)) \
		return; \
	}

// wren functions

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

// hashing functions

static uint64_t hash_str(char const* str) { // djb2 algorithm
	uint64_t hash = 5381;
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++) {
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]
	}

	return hash;
}
