#pragma once

// useful macros

#define BIND_FOREIGN_METHOD(_static_, _signature, fn) \
	if (static_ == (_static_) && !strcmp(signature, (_signature))) { \
		return (fn); \
	}

#define CHECK_ARGC(fn_name, argc_little, argc_big) \
	char const* const __fn_name = (fn_name); /* accessibly by future macros */ \
	int argc = wrenGetSlotCount(vm) - 1; \
	\
	if (argc < (argc_little) || argc > (argc_big)) { \
		if ((argc_little) == (argc_big)) { \
			LOG_WARN("'%s' not passed right number of arguments (got %d, expected between %d & %d)", __fn_name, argc, (argc_little), (argc_big)) \
		} \
		\
		else { \
			LOG_WARN("'%s' not passed right number of arguments (got %d, expected %d)", __fn_name, argc, (argc_little)) \
		} \
		\
		return; \
	}

#define ASSERT_ARG_TYPE(i, type) \
	if (wrenGetSlotType(vm, (i)) != (type)) { \
		LOG_WARN("'%s' argument " #i " is of incorrect type", __fn_name) \
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

// exec args stack object

typedef struct {
	size_t len; // includes NULL sentinel
	char** args;
} exec_args_t;

static exec_args_t* exec_args_new(int len, ...) {
	va_list va;
	va_start(va, len);

	exec_args_t* self = calloc(1, sizeof *self);

	self->len = len + 1;
	self->args = calloc(self->len, sizeof *self->args);

	for (size_t i = 0; i < len; i++) {
		self->args[i] = strdup(va_arg(va, char*));
	}

	va_end(va);
	return self;
}

static void exec_args_add(exec_args_t* self, char* arg) {
	self->args = realloc(self->args, ++self->len * sizeof *self->args);

	self->args[self->len - 2] = strdup(arg);
	self->args[self->len - 1] = NULL;
}

__attribute__((__format__(__printf__, 2, 3)))
static void exec_args_fmt(exec_args_t* self, char* fmt, ...) {
	va_list va;
	va_start(va, fmt);

	self->args = realloc(self->args, ++self->len * sizeof *self->args);

	vasprintf(&self->args[self->len - 2], fmt, va);
	self->args[self->len - 1] = NULL;

	va_end(va);
}

static void exec_args_print(exec_args_t* self) {
	printf("exec_args(%p) = {\n", self);

	for (size_t i = 0; i < self->len - 1 /* don't free NULL sentinel */; i++) {
		char* const arg = self->args[i];

		if (!arg) { // shouldn't happen but let's be defensive...
			continue;
		}

		printf("\t\"%s\",\n", arg);
	}

	printf("}\n");
}

static void exec_args_del(exec_args_t* self) {
	for (size_t i = 0; i < self->len - 1 /* don't free NULL sentinel */; i++) {
		char* const arg = self->args[i];

		if (!arg) { // shouldn't happen but let's be defensive...
			continue;
		}

		free(arg);
	}

	free(self->args);
	free(self);
}

// process manipulation functions

static int wait_for_process(pid_t pid) {
	int wstatus = 0;
	while (waitpid(pid, &wstatus, 0) > 0);

	if (WIFSIGNALED(wstatus)) {
		return -1;
	}

	if (WIFEXITED(wstatus)) {
		return WEXITSTATUS(wstatus);
	}

	return 0;
}

static pid_t execute_async(exec_args_t* _exec_args) {
	char** exec_args = _exec_args->args;
	pid_t pid = fork();

	if (!pid) {
		if (execv(exec_args[0], exec_args) < 0) {
			LOG_FATAL("execv(\"%s\"): %s", exec_args[0], strerror(errno))
		}

		_exit(EXIT_FAILURE);
	}

	return pid;
}

static int execute(exec_args_t* _exec_args) {
	pid_t pid = execute_async(_exec_args);
	return wait_for_process(pid);
}
