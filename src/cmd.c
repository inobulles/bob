// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <cmd.h>
#include <common.h>
#include <logging.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_create(cmd_t* cmd, ...) {
	va_list va;
	va_start(va, cmd);

	cmd->len = 1;
	cmd->args = calloc(1, sizeof *cmd->args);
	assert(cmd->args != NULL);

	for (;;) {
		char* const next = va_arg(va, char*);

		if (next == NULL) {
			break;
		}

		cmd->args = realloc(cmd->args, ++cmd->len * sizeof *cmd->args);
		assert(cmd->args != NULL);

		cmd->args[cmd->len - 2] = strdup(next);
		assert(cmd->args[cmd->len - 2] != NULL);

		cmd->args[cmd->len - 1] = NULL;
	}

	va_end(va);
	return 0;
}

__attribute__((__format__(__printf__, 2, 3))) void cmd_addf(cmd_t* cmd, char const* fmt, ...) {
	va_list va;
	va_start(va, fmt);

	cmd->args = realloc(cmd->args, ++cmd->len * sizeof *cmd->args);
	assert(cmd->args != NULL);

	vasprintf(&cmd->args[cmd->len - 2], fmt, va);
	assert(cmd->args[cmd->len - 2] != NULL);
	cmd->args[cmd->len - 1] = NULL;

	va_end(va);
}

pid_t cmd_exec_async(cmd_t* cmd) {
	// Create pipes.

	int fd[2];

	if (pipe(fd) < 0) {
		LOG_FATAL("pipe: %s", strerror(errno));
		return -1;
	}

	cmd->in = fd[1];
	cmd->out = fd[0];

	// Find and start process.

	pid_t const pid = fork();

	if (pid < 0) {
		LOG_ERROR("fork: %s", strerror(errno));
		return -1;
	}

	char** const args = cmd->args;

	if (!pid) {
		// Handle pipe (child).
		// Close input side of pipe if it exists, as we wanna send output.
		// Then, redirect 'stdout'/'stderr' of process to pipe input.
		// TODO the above two comments are clearly wrong.

		close(cmd->out);

		for (size_t fileno = STDOUT_FILENO; fileno <= STDERR_FILENO; fileno++) {
			if (dup2(cmd->in, fileno) < 0) {
				LOG_ERROR("dup2(%d, %d): %s", cmd->in, fileno, strerror(errno));
				_exit(EXIT_FAILURE);
			}
		}

		// Attempt first to execute at the path passed.

		if (!execv(args[0], args)) {
			_exit(EXIT_SUCCESS);
		}

		// If we can't, search for a binary in our '$PATH'.
		// Only take into account the last component of the query path.

		char* query = strrchr(args[0], '/');

		if (query == NULL) {
			query = args[0];
		}

		char* const path = getenv("PATH");

		if (path == NULL) {
			LOG_FATAL("getenv(\"PATH\"): couldn't find '$PATH'");
			_exit(EXIT_FAILURE);
		}

		char* CLEANUP_STR search = strdup(getenv("PATH"));
		assert(search != NULL);

		char* tok;

		while ((tok = strsep(&search, ":"))) {
			char* CLEANUP_STR path = NULL;
			asprintf(&path, "%s/%s", tok, query);
			assert(path != NULL);

			args[0] = path;

			if (!execv(args[0], args)) {
				_exit(EXIT_SUCCESS);
			}
		}

		// Error if all else fails.

		LOG_FATAL("execv(\"%s\" and searched in PATH): %s", query, strerror(errno));
		_exit(EXIT_FAILURE);
	}

	// Handle pipe (parent).

	close(cmd->in);
	return pid;
}

static int wait_for_process(pid_t pid) {
	int wstatus = 0;
	while (waitpid(pid, &wstatus, 0) > 0)
		;

	if (WIFSIGNALED(wstatus)) {
		return -1;
	}

	if (WIFEXITED(wstatus)) {
		return WEXITSTATUS(wstatus) == EXIT_SUCCESS ? 0 : -1;
	}

	return 0;
}

int cmd_exec(cmd_t* cmd) {
	pid_t const pid = cmd_exec_async(cmd);

	if (pid < 0) {
		return -1;
	}

	return wait_for_process(pid);
}

char* cmd_read_out(cmd_t* cmd) {
	int const pipe = cmd->out;

	char* out = strdup("");
	assert(out != NULL);

	size_t total = 0;

	char chunk[4096];
	ssize_t bytes;

	while ((bytes = read(pipe, chunk, sizeof chunk)) > 0) {
		total += bytes;

		out = realloc(out, total + 1);
		assert(out != NULL);
		out[total] = '\0';

		memcpy(out + total - bytes, chunk, bytes);
	}

	if (bytes < 0) {
		LOG_WARN("%s: Failed to read from %d: %s", __func__, pipe, strerror(errno));
	}

	out[total] = '\0';
	return out;
}

__attribute__((unused)) void cmd_print(cmd_t* cmd) {
	printf("cmd(%p) = {\n", cmd);

	for (size_t i = 0; i < cmd->len - 1 /* Don't print NULL sentinel. */; i++) {
		char* const arg = cmd->args[i];

		if (!arg) { // Shouldn't happen but let's be defensive...
			continue;
		}

		printf("\t\"%s\",\n", arg);
	}

	printf("}\n");
}

void cmd_free(cmd_t* cmd) {
	for (size_t i = 0; i < cmd->len - 1 /* Don't free NULL sentinel. */; i++) {
		char* const arg = cmd->args[i];

		if (arg == NULL) { // Shouldn't happen but let's be defensive...
			continue;
		}

		free(arg);
	}

	if (cmd->args != NULL) {
		free(cmd->args);
	}

	cmd->len = 0;
	cmd->args = NULL;
}
