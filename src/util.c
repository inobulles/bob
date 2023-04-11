#include <err.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/wait.h>

#include "logging.h"

#include "util.h"

// wren functions

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

// pipe helpers

void pipe_create(pipe_t* self) {
	// it's important to use '-1' as a default value, because theoretically, file descriptor 0 doesn't *have* to be stdout

	self->in  = -1;
	self->out = -1;

	self->err_in  = -1;
	self->err_out = -1;

	int fd[2];

	// yeah, not super good design to have arguments passed as structure members but whatever yo

	if (self->kind & PIPE_STDOUT) {
		if (pipe(fd) < 0)
			errx(EXIT_FAILURE, "pipe: %s", strerror(errno));

		self->in  = fd[1];
		self->out = fd[0];
	}

	if (self->kind & PIPE_STDERR) {
		if (pipe(fd) < 0)
			errx(EXIT_FAILURE, "pipe: %s", strerror(errno));

		self->err_in  = fd[1];
		self->err_out = fd[0];
	}
}

void pipe_child(pipe_t* self) {
	// close input side of pipe if it exists, as we wanna send output
	// then, redirect stdout/stderr of process to pipe input

	if (self->kind & PIPE_STDOUT) {
		close(self->out);

		if (dup2(self->in, STDOUT_FILENO) < 0)
			errx(EXIT_FAILURE, "dup2(%d, STDOUT_FILENO): %s", self->in, strerror(errno));
	}

	if (self->kind & PIPE_STDERR) {
		close(self->err_out);

		if (dup2(self->err_in, STDERR_FILENO) < 0)
			errx(EXIT_FAILURE, "dup2(%d, STDERR_FILENO): %s", self->err_in, strerror(errno));
	}
}

void pipe_parent(pipe_t* self) {
	if (self->kind & PIPE_STDOUT) {
		close(self->in);
		self->in = -1;
	}

	if (self->kind & PIPE_STDERR) {
		close(self->err_in);
		self->err_in = -1;
	}
}

char* pipe_read_out(pipe_t* self, pipe_kind_t kind) {
	// make sure everything is as we expect

	int pipe = self->out;

	if (kind == PIPE_STDERR)
		pipe = self->err_out;

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

	if (bytes < 0)
		LOG_WARN("pipe_read_out: Failed to read from %d: %s", pipe, strerror(errno))

	out[total] = '\0';
	return out;
}

void pipe_free(pipe_t* self) {
	if (self->in >= 0)
		close(self->in);

	if (self->out >= 0)
		close(self->out);

	if (self->err_in >= 0)
		close(self->err_in);

	if (self->err_out >= 0)
		close(self->err_out);
}

// exec args stack object

exec_args_t* exec_args_new(size_t len, ...) {
	va_list va;
	va_start(va, len);

	exec_args_t* self = calloc(1, sizeof *self);

	self->len = len + 1;
	self->args = calloc(self->len, sizeof *self->args);

	for (size_t i = 0; i < len; i++)
		self->args[i] = strdup(va_arg(va, char*));

	va_end(va);
	return self;
}

void exec_args_save_out(exec_args_t* self, pipe_kind_t kind) {
	self->pipe.kind = kind;
}

char* exec_args_read_out(exec_args_t* self, pipe_kind_t kind) {
	return pipe_read_out(&self->pipe, kind);
}

void exec_args_add(exec_args_t* self, char const* arg) {
	self->args = realloc(self->args, ++self->len * sizeof *self->args);

	self->args[self->len - 2] = strdup(arg);
	self->args[self->len - 1] = NULL;
}

__attribute__((__format__(__printf__, 2, 3)))
void exec_args_fmt(exec_args_t* self, char const* fmt, ...) {
	va_list va;
	va_start(va, fmt);

	self->args = realloc(self->args, ++self->len * sizeof *self->args);

	vasprintf(&self->args[self->len - 2], fmt, va);
	self->args[self->len - 1] = NULL;

	va_end(va);
}

__attribute__((unused))
void exec_args_print(exec_args_t* self) {
	printf("exec_args(%p) = {\n", self);

	for (size_t i = 0; i < self->len - 1 /* don't free NULL sentinel */; i++) {
		char* const arg = self->args[i];

		if (!arg) // shouldn't happen but let's be defensive...
			continue;

		printf("\t\"%s\",\n", arg);
	}

	printf("}\n");
}

void exec_args_del(exec_args_t* self) {
	for (size_t i = 0; i < self->len - 1 /* don't free NULL sentinel */; i++) {
		char* const arg = self->args[i];

		if (!arg) // shouldn't happen but let's be defensive...
			continue;

		free(arg);
	}

	if (self->args)
		free(self->args);

	pipe_free(&self->pipe);
	free(self);
}

// process manipulation functions

int wait_for_process(pid_t pid) {
	int wstatus = 0;
	while (waitpid(pid, &wstatus, 0) > 0);

	if (WIFSIGNALED(wstatus))
		return EXIT_FAILURE;

	if (WIFEXITED(wstatus))
		return WEXITSTATUS(wstatus);

	return EXIT_SUCCESS;
}

pid_t execute_async(exec_args_t* self) {
	char** exec_args = self->args;
	pipe_create(&self->pipe);

	// fork process

	pid_t pid = fork();

	if (pid < 0)
		errx(EXIT_FAILURE, "fork: %s", strerror(errno));

	if (!pid) {
		pipe_child(&self->pipe);

		// attempt first to execute at the path passed

		if (!execv(exec_args[0], exec_args))
			_exit(EXIT_SUCCESS);

		// if we can't, search for a binary in our PATH
		// only take into account the last component of the query path

		char* query = strrchr(exec_args[0], '/');

		if (!query)
			query = exec_args[0];

		char* search = strdup(getenv("PATH"));
		char* tok;

		while ((tok = strsep(&search, ":"))) {
			char* path;
			if (asprintf(&path, "%s/%s", tok, query)) {}

			exec_args[0] = path;

			if (!execv(exec_args[0], exec_args))
				_exit(EXIT_SUCCESS);

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

int execute(exec_args_t* _exec_args) {
	pid_t pid = execute_async(_exec_args);
	return wait_for_process(pid);
}

// filesystem functions

size_t file_get_size(FILE* fp) {
	fseek(fp, 0, SEEK_END);
	size_t const size = ftell(fp);
	rewind(fp);

	return size;
}

char* file_read_str(FILE* fp, size_t size) {
	char* const str = malloc(size + 1);

	if (fread(str, 1, size, fp)) {}

	str[size] = 0;
	return str;
}

int mkdir_recursive(char const* _path) {
	int rv = -1;

	// we don't need to do anything if path is empty

	if (!*_path)
		return 0;

	// remember previous working directory, because to make our lives easier, we'll be jumping around the place to create our subdirectories

	char* const __attribute__((cleanup(strfree))) cwd = getcwd(NULL, 0);

	if (!cwd) {
		LOG_ERROR("getcwd: %s", strerror(errno))
		goto err_cwd;
	}

	char* path = strdup(_path);

	// if we're dealing with a path relative to $HOME, chdir to $HOME first

	if (*path == '~') {
		char* const home = getenv("HOME");

		// if $HOME isn't set, treat as an absolute directory

		if (!home)
			*path = '/';

		else if (chdir(home) < 0) {
			LOG_ERROR("chdir($HOME): %s", strerror(errno))
			goto err_home;
		}
	}

	// if we're dealing with an absolute path, chdir to '/' and treat path as relative

	if (*path == '/' && chdir("/") < 0) {
		LOG_ERROR("chdir(\"/\"): %s", strerror(errno))
		goto err_abs;
	}

	// parse the path itself

	char* bit;

	while ((bit = strsep(&path, "/"))) {
		// ignore if the bit is empty

		if (!bit || !*bit)
			continue;

		// ignore if the bit refers to the current directory

		if (!strcmp(bit, "."))
			continue;

		// don't attempt to mkdir if we're going backwards, only chdir

		if (!strcmp(bit, ".."))
			goto no_mkdir;

		if (mkdir(bit, 0755) < 0 && errno != EEXIST) {
			LOG_ERROR("mkdir(\"%s\"): %s", bit, strerror(errno))
			goto err_mkdir;
		}

	no_mkdir:

		if (chdir(bit) < 0) {
			LOG_ERROR("chdir(\"%s\"): %s", bit, strerror(errno))
			goto err_chdir;
		}
	}

	// success

	rv = 0;

err_chdir:
err_mkdir:

	// move back to current directory once we're sure the output directory exists (or there's an error)

	if (chdir(cwd) < 0)
		LOG_ERROR("chdir(\"%s\"): %s", cwd, strerror(errno))

err_abs:
err_home:

	free(path);

err_cwd:

	return rv;
}

int copy_recursive(char const* _src, char const* dest) {
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

	if (add_slash)
		if (asprintf(&src, "%s/", src)) {};

	exec_args_t* exec_args = exec_args_new(4, "cp", "-RpP", src, dest);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	if (add_slash) 
		free(src);

	return rv;
}

int remove_recursive(char const* path) {
	// same comment as for 'copy'

	exec_args_t* exec_args = exec_args_new(3, "rm", "-rf", path);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	return rv;
}

// misc stuff

void strfree(char* const* str_ref) {
	char* const str = *str_ref;

	if (!str)
		return;

	free(str);
}

uint64_t hash_str(char const* str) { // djb2 algorithm
	uint64_t hash = 5381;
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++)
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]

	return hash;
}

void navigate_project_path(void) {
	// navigate into project directory, if one was specified

	if (project_path && chdir(project_path) < 0)
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", project_path, strerror(errno));
}

void ensure_out_path(void) {
	// make sure output directory exists - create it if it doesn't

	if (mkdir_recursive(rel_bin_path) < 0)
		errx(EXIT_FAILURE, "mkdir_recursive(\"%s\"): %s", rel_bin_path, strerror(errno));

	// get absolute path of output directory so we don't ever get lost or confused

	bin_path = realpath(rel_bin_path, NULL);

	if (!bin_path)
		errx(EXIT_FAILURE, "realpath(\"%s\"): %s", rel_bin_path, strerror(errno));
}

char const* install_prefix(void) {
	if (prefix)
		return prefix;

	// if on FreeBSD/aquaBSD (and, to be safe, anywhere else), the prefix will be '/usr/local'
	// on Linux, it will simply be '/usr'

#if defined(__linux__)
	return "/usr";
#endif

	return "/usr/local";
}
