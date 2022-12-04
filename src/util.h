#pragma once

// useful macros

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

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
		LOG_WARN("'%s' argument " #i " is of incorrect type (expected '" #type "')", __fn_name) \
		return; \
	}

// kinda replicate the umber API

#define CLEAR   "\033[0m"
#define REGULAR "\033[0;"
#define BOLD    "\033[1;"

#define PURPLE  "35m"
#define RED     "31m"
#define YELLOW  "33m"
#define GREEN   "32m"

__attribute__((__format__(__printf__, 3, 0)))
void vlog(FILE* stream, char const* colour, char const* const fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char* msg;
	vasprintf(&msg, fmt, args);

	va_end(args);

	fprintf(stream, "%s%s%s\n", colour, msg, CLEAR);
	free(msg);
}

#define LOG_FATAL(...)   vlog(stderr, "💀 " BOLD    PURPLE, __VA_ARGS__);
#define LOG_ERROR(...)   vlog(stderr, "🔴 " BOLD    RED,    __VA_ARGS__);
#define LOG_WARN(...)    vlog(stderr, "⚠️  " REGULAR YELLOW, __VA_ARGS__);
#define LOG_SUCCESS(...) vlog(stderr, "🟢 " REGULAR GREEN,  __VA_ARGS__);

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

static void exec_args_add(exec_args_t* self, char const* arg) {
	self->args = realloc(self->args, ++self->len * sizeof *self->args);

	self->args[self->len - 2] = strdup(arg);
	self->args[self->len - 1] = NULL;
}

__attribute__((__format__(__printf__, 2, 3)))
static void exec_args_fmt(exec_args_t* self, char const* fmt, ...) {
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
		return EXIT_FAILURE;
	}

	if (WIFEXITED(wstatus)) {
		return WEXITSTATUS(wstatus);
	}

	return EXIT_SUCCESS;
}

static pid_t execute_async(exec_args_t* _exec_args) {
	char** exec_args = _exec_args->args;
	pid_t pid = fork();

	if (!pid) {
		// attempt first to execute at the path passed

		if (!execv(exec_args[0], exec_args)) {
			_exit(EXIT_SUCCESS);
		}

		// if we can't, search for a binary in our PATH
		// only take into account the last component of the query path

		char* query = strrchr(exec_args[0], '/');

		if (!query) {
			query = exec_args[0];
		}

		char* search = strdup(getenv("PATH"));
		char* tok;

		while ((tok = strsep(&search, ":"))) {
			char* path;

			if (asprintf(&path, "%s/%s", tok, query))
				;

			exec_args[0] = path;

			if (!execv(exec_args[0], exec_args)) {
				_exit(EXIT_SUCCESS);
			}

			free(path);
		}

		// error if all else fails

		LOG_FATAL("execv(\"%s\" and searched in PATH): %s", query, strerror(errno))
		_exit(EXIT_FAILURE);
	}

	return pid;
}

static int execute(exec_args_t* _exec_args) {
	pid_t pid = execute_async(_exec_args);
	return wait_for_process(pid);
}

// filesystem functions

static size_t file_get_size(FILE* fp) {
	fseek(fp, 0, SEEK_END);
	size_t const size = ftell(fp);
	rewind(fp);

	return size;
}

static char* file_read_str(FILE* fp, size_t size) {
	char* const str = malloc(size + 1);

	if (fread(str, 1, size, fp))
		;

	str[size] = 0;
	return str;
}

static int copy_recursive(char const* src, char const* dest) {
	// it's unfortunate, but to be as cross-platform as possible, we must shell out execution to the 'cp' binary
	// would've loved to use libcopyfile but, alas, POSIX is missing features :(

	exec_args_t* exec_args = exec_args_new(4, "cp", "-RpP", src, dest);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	return rv;
}

static int remove_recursive(char const* path) {
	// same comment as for 'copy'

	exec_args_t* exec_args = exec_args_new(3, "rm", "-rf", path);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	return rv;
}
