#include <util.h>

#include <string.h>

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
