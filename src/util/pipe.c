#include <util.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int pipe_create(pipe_t* self) {
	// it's important to use '-1' as a default value, because theoretically, file descriptor 0 doesn't *have* to be stdout

	self->in  = -1;
	self->out = -1;

	self->err_in  = -1;
	self->err_out = -1;

	int fd[2];

	// yeah, not super good design to have arguments passed as structure members but whatever yo

	if (self->kind & PIPE_STDOUT) {
		if (pipe(fd) < 0) {
			LOG_ERROR("pipe: %s", strerror(errno))
			goto err;
		}

		self->in  = fd[1];
		self->out = fd[0];
	}

	if (self->kind & PIPE_STDERR) {
		if (pipe(fd) < 0) {
			LOG_ERROR("pipe: %s", strerror(errno))
			goto err;
		}

		self->err_in  = fd[1];
		self->err_out = fd[0];
	}

	return 0;

err:

	pipe_free(self);
	return -1;
}

void pipe_child(pipe_t* self) {
	// close input side of pipe if it exists, as we wanna send output
	// then, redirect stdout/stderr of process to pipe input

	if (self->kind & PIPE_STDOUT) {
		close(self->out);

		if (dup2(self->in, STDOUT_FILENO) < 0) {
			LOG_ERROR("dup2(%d, STDOUT_FILENO): %s", self->in, strerror(errno))
			_exit(EXIT_FAILURE);
		}
	}

	if (self->kind & PIPE_STDERR) {
		close(self->err_out);

		if (dup2(self->err_in, STDERR_FILENO) < 0) {
			LOG_ERROR("dup2(%d, STDERR_FILENO): %s", self->err_in, strerror(errno))
			_exit(EXIT_FAILURE);
		}
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
