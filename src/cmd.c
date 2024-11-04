// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <cmd.h>
#include <common.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cmd_create(cmd_t* cmd, ...) {
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

	cmd->in = -1;
	cmd->out = -1;
}

void cmd_add(cmd_t* cmd, char const* arg) {
	cmd->args = realloc(cmd->args, ++cmd->len * sizeof *cmd->args);
	assert(cmd->args != NULL);

	cmd->args[cmd->len - 2] = strdup(arg);
	assert(cmd->args[cmd->len - 2] != NULL);
	cmd->args[cmd->len - 1] = NULL;
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

int cmd_exec_inplace(cmd_t* cmd) {
	// Attempt first to execute at the path passed.
	// "If any of the exec() functions returns, an error will have occurred."

	execv(cmd->args[0], cmd->args);

	// If we can't, search for a binary in our '$PATH'.
	// Only take into account the last component of the query path.

	char* query = strrchr(cmd->args[0], '/');

	if (query == NULL) {
		query = cmd->args[0];
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

		char* const prev = cmd->args[0];
		cmd->args[0] = path;
		execv(cmd->args[0], cmd->args);
		cmd->args[0] = prev;
	}

	// Error if all else fails.

	LOG_FATAL("execv(\"%s\" and searched in PATH): %s", query, strerror(errno));
	return -1;
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

	if (!pid) {
		// Handle pipe (child).
		// Close output side of pipe, as we're the ones writing to it
		// Then, redirect 'stdout'/'stderr' of process to pipe input.

		close(cmd->out);

		for (size_t fileno = STDOUT_FILENO; fileno <= STDERR_FILENO; fileno++) {
			if (dup2(cmd->in, fileno) < 0) {
				LOG_ERROR("dup2(%d, %d): %s", cmd->in, fileno, strerror(errno));
				_exit(EXIT_FAILURE);
			}
		}

		// Replace this process.

		_exit(cmd_exec_inplace(cmd) < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
	}

	// Handle pipe (parent).
	// Just close input side of the pipe, as we just read from the output.

	close(cmd->in);
	cmd->in = -1;

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

	cmd->rv = wait_for_process(pid);
	return cmd->rv;
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

void cmd_log(cmd_t* cmd, char const* cookie, char const* prefix, char const* infinitive, char const* past) {
	char* const CLEANUP_STR out = cmd_read_out(cmd);
	bool const is_out = out[0] != '\0';
	char* const suffix = is_out ? ":" : ".";

#define P prefix ? prefix : "", prefix ? ": " : ""

	if (cmd->rv < 0) {
		LOG_ERROR("%s" CLEAR "%sFailed to %s%s", P, infinitive, suffix);
	}

	else {
		LOG_SUCCESS("%s" CLEAR "%sSuccessfully %s%s", P, past, suffix);
	}

#undef P

	printf("%s", out);

	// Write out log to file, only if there is one.
	// If not, attempt to remove the existing one anyway.

	char* CLEANUP_STR path;
	asprintf(&path, "%s.log", cookie);
	assert(path != NULL);

	if (!is_out) {
		remove(path);
		return;
	}

	FILE* const f = fopen(path, "w");

	if (f == NULL) {
		LOG_WARN("fopen(\"%s\", \"w\"): %s", path, strerror(errno));
		return;
	}

	fprintf(f, "%s", out);
	fclose(f);
}

__attribute__((unused)) void cmd_print(cmd_t* cmd) {
	printf("cmd(%p) = {\n", cmd);

	for (size_t i = 0; i < cmd->len - 1 /* Don't print NULL sentinel. */; i++) {
		printf("\t\"%s\",\n", cmd->args[i]);
	}

	printf("}\n");
}

void cmd_free(cmd_t* cmd) {
	for (ssize_t i = 0; i < (ssize_t) cmd->len - 1 /* Don't free NULL sentinel. */; i++) {
		char* const arg = cmd->args[i];
		assert(arg != NULL);
		free(arg);
	}

	if (cmd->args != NULL) {
		free(cmd->args);
	}

	cmd->len = 0;
	cmd->args = NULL;

	if (cmd->in >= 0) {
		close(cmd->in);
	}

	if (cmd->out >= 0) {
		close(cmd->out);
	}
}
