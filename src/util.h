#pragma once

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/wait.h>

// XXX can't do this right now, because clangd doesn't know relative to where the '-Iwren/include' options is
//     the solution will be to generate a 'clang_commands.json' file automatically I believe :)

//#include <wren.h>
#include "wren/include/wren.h"

#include "logging.h"

// global variables

extern char* bin_path;
extern char const* init_name;
extern char const* curr_instr;
extern char const* prefix;

// useful macros

#define BIND_FOREIGN_METHOD(_static_, _signature, fn) \
	if (static_ == (_static_) && !strcmp(signature, (_signature))) { \
		return (fn); \
	}

// this may be a contender for ugliest macro I've ever written
// not sure though, I've written some pretty ugly macros in my time

#define CHECK_ARGC(fn_name, argc_little, argc_big) \
	char const* const __fn_name = (fn_name); /* accessible by future macros */ \
	int argc = wrenGetSlotCount(vm) - 1; \
	\
	/* be pessimistic and set return value to null right off the bat */ \
	/* foreign values in slot 0 really come in to put a thorn in our side, but that's alright, we too cool for that to stop us 😎 */ \
	/* getting the value of the foreign slot doesn't work when calling 'wrenSetSlotNewForeign' though, because we can't just read what's in 'classSlot', so we'll have to make an exception to '.new' methods */ \
	\
	__attribute__((unused)) void* foreign = NULL; \
	\
	if (strcmp(strrchr(__fn_name, '.'), ".new")) { \
		if (wrenGetSlotType(vm, 0) == WREN_TYPE_FOREIGN) { \
			foreign = wrenGetSlotForeign(vm, 0); \
		} \
		\
		wrenSetSlotNull(vm, 0); \
	} \
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

// pipe helpers

typedef enum {
	PIPE_STDOUT = 0b01,
	PIPE_STDERR = 0b10,
} pipe_kind_t;

typedef struct {
	pipe_kind_t kind;

	int in;
	int out;

	int err_in;
	int err_out;
} pipe_t;

static void pipe_create(pipe_t* self) {
	// it's important to use '-1' as a default value, because theoretically, file descriptor 0 doesn't *have* to be stdout

	self->in  = -1;
	self->out = -1;

	self->err_in  = -1;
	self->err_out = -1;

	int fd[2];

	// yeah, not super good design to have arguments passed as structure members but whatever yo

	if (self->kind & PIPE_STDOUT) {
		if (pipe(fd) < 0) {
			errx(EXIT_FAILURE, "pipe: %s", strerror(errno));
		}

		self->in  = fd[1];
		self->out = fd[0];
	}

	if (self->kind & PIPE_STDERR) {
		if (pipe(fd) < 0) {
			errx(EXIT_FAILURE, "pipe: %s", strerror(errno));
		}

		self->err_in  = fd[1];
		self->err_out = fd[0];
	}
}

static void pipe_child(pipe_t* self) {
	// close input side of pipe if it exists, as we wanna send output
	// then, redirect stdout/stderr of process to pipe input

	if (self->kind & PIPE_STDOUT) {
		close(self->out);

		if (dup2(self->in, STDOUT_FILENO) < 0) {
			errx(EXIT_FAILURE, "dup2(%d, STDOUT_FILENO): %s", self->in, strerror(errno));
		}
	}

	if (self->kind & PIPE_STDERR) {
		close(self->err_out);

		if (dup2(self->err_in, STDERR_FILENO) < 0) {
			errx(EXIT_FAILURE, "dup2(%d, STDERR_FILENO): %s", self->err_in, strerror(errno));
		}
	}
}

static void pipe_parent(pipe_t* self) {
	if (self->kind & PIPE_STDOUT) {
		close(self->in);
		self->in = -1;
	}

	if (self->kind & PIPE_STDERR) {
		close(self->err_in);
		self->err_in = -1;
	}
}

static char* pipe_read_out(pipe_t* self, pipe_kind_t kind) {
	// make sure everything is as we expect

	int pipe = self->out;

	if (kind == PIPE_STDERR) {
		pipe = self->err_out;
	}

	else if (kind != PIPE_STDOUT) {
		LOG_ERROR("pipe_read_out: Unknown save output kind value %d", kind)
		return NULL;
	}

	if (pipe < 0) {
		LOG_ERROR("pipe_read_out: Trying to read from nonexistent pipe - did you run 'exec_kind'?")
		return NULL;
	}

	// start reading

	char* out = strdup("");
	size_t total = 0;

	char chunk[4096];
	ssize_t bytes;

	while ((bytes = read(pipe, chunk, sizeof chunk)) > 0) {
		total += bytes;

		out = realloc(out, total + 1);
		out[total] = '\0';

		memcpy(out + total - bytes, chunk, bytes);
	}

	if (bytes < 0) {
		LOG_WARN("pipe_read_out: Failed to read from %d: %s", pipe, strerror(errno))
	}

	out[total] = '\0';
	return out;
}

static void pipe_free(pipe_t* self) {
	if (self->in >= 0) {
		close(self->in);
	}

	if (self->out >= 0) {
		close(self->out);
	}

	if (self->err_in >= 0) {
		close(self->err_in);
	}

	if (self->err_out >= 0) {
		close(self->err_out);
	}
}

// exec args stack object

typedef struct {
	size_t len; // includes NULL sentinel
	char** args;

	pipe_t pipe;
} exec_args_t;

static exec_args_t* exec_args_new(size_t len, ...) {
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

static void exec_args_save_out(exec_args_t* self, pipe_kind_t kind) {
	self->pipe.kind = kind;
}

static char* exec_args_read_out(exec_args_t* self, pipe_kind_t kind) {
	return pipe_read_out(&self->pipe, kind);
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

__attribute__((unused))
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

	if (self->args) {
		free(self->args);
	}

	pipe_free(&self->pipe);
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

static pid_t execute_async(exec_args_t* self) {
	char** exec_args = self->args;
	pipe_create(&self->pipe);

	// fork process

	pid_t pid = fork();

	if (pid < 0) {
		errx(EXIT_FAILURE, "fork: %s", strerror(errno));
	}

	if (!pid) {
		pipe_child(&self->pipe);

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
			if (asprintf(&path, "%s/%s", tok, query)) {}

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

	// we're the parent
	// close the output side of the pipes if they exist, as we wanna receive input

	pipe_parent(&self->pipe);
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

	if (fread(str, 1, size, fp)) {}

	str[size] = 0;
	return str;
}

static int mkdir_recursive(char const* _path) {
	// TODO what about '~' in paths?
	// TODO proper error handling

	int rv = -1;

	// remember previous working directory, because to make our lives easier, we'll be jumping around the place to create our subdirectories

	char* const cwd = getcwd(NULL, 0);

	if (!cwd) {
		errx(EXIT_FAILURE, "getcwd: %s", strerror(errno));
	}

	char* path = strdup(_path);

	// if we're dealing with an absolute path, chdir to '/' and treat path as relative

	if (*path == '/') {
		while (*++path == '/'); // remove prepending slashes, however many there may be

		if (chdir("/") < 0) {
			errx(EXIT_FAILURE, "chdir(\"/\"): %s", strerror(errno));
		}
	}

	// parse the path itself

	char* bit;

	while ((bit = strsep(&path, "/"))) {
		// ignore if the bit is empty

		if (!bit || !*bit) {
			continue;
		}

		// ignore if the bit refers to the current directory

		if (!strcmp(bit, ".")) {
			continue;
		}

		// don't attempt to mkdir if we're going backwards, only chdir

		if (!strcmp(bit, "..")) {
			goto no_mkdir;
		}

		if (mkdir(bit, 0700) < 0 && errno != EEXIST) {
			errx(EXIT_FAILURE, "mkdir(\"%s\"): %s", bit, strerror(errno));
		}

	no_mkdir:

		if (chdir(bit) < 0) {
			errx(EXIT_FAILURE, "chdir(\"%s\"): %s", bit, strerror(errno));
		}
	}

	// move back to current directory once we're sure the output directory exists

	if (chdir(cwd) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", cwd, strerror(errno));
	}

	// success

	rv = 0;

	free(path);
	free(cwd);

	return rv;
}

static int copy_recursive(char const* _src, char const* dest) {
	// it's unfortunate, but to be as cross-platform as possible, we must shell out execution to the 'cp' binary
	// would've loved to use libcopyfile but, alas, POSIX is missing features :(
	// if 'src' is a directory, append a slash to it to override stupid cp(1) behaviour

	struct stat sb;

	if (stat(_src, &sb) < 0) {
		LOG_ERROR("stat(\"%s\"): %s", _src, strerror(errno))
		return EXIT_FAILURE;
	}

	bool const add_slash = S_ISDIR(sb.st_mode);
	char* src = (void*) _src;

	if (add_slash) {
		if (asprintf(&src, "%s/", src)) {};
	}

	exec_args_t* exec_args = exec_args_new(4, "cp", "-RpP", src, dest);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	if (add_slash) {
		free(src);
	}

	return rv;
}

static int remove_recursive(char const* path) {
	// same comment as for 'copy'

	exec_args_t* exec_args = exec_args_new(3, "rm", "-rf", path);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	return rv;
}

// misc stuff

static char const* install_prefix(void) {
	if (prefix) {
		return prefix;
	}

	// if on FreeBSD/aquaBSD (and, to be safe, anywhere else), the prefix will be '/usr/local'
	// on Linux, it will simply be '/usr'

#if defined(__linux__)
	return "/usr";
#endif

	return "/usr/local";
}
