// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2025 Aymeric Wibo

#include <common.h>

#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

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

	cmd_set_redirect(cmd, true, false);

	cmd->out_buf = NULL;
	cmd->in = -1;
	cmd->out = -1;

	cmd->pending_stdin = NULL;
	cmd->stdin_in = -1;
	cmd->stdin_out = -1;
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

void cmd_add_argv(cmd_t* cmd, int argc, char* argv[]) {
	for (ssize_t i = 0; i < argc; i++) {
		// On BSD/macOS, getopt doesn't consume the '--' argument.

		if (strcmp(argv[i], "--") == 0) {
			continue;
		}

		cmd_add(cmd, argv[i]);
	}
}

void cmd_set_redirect(cmd_t* cmd, bool redirect, bool force) {
	// We don't do any of this output pipe stuff if we're debugging the build, because we want to see the outputs of commands in real-time before they terminate.
	// Except if we necessarily need redirect (e.g. PkgConfig, Cc deps)!

	if (debugging && !force) {
		cmd->redirect = false;
		return;
	}

	cmd->redirect = redirect;
}

void cmd_prepare_stdin(cmd_t* cmd, char* data) {
	cmd->pending_stdin = strdup(data);
	assert(cmd->pending_stdin != NULL);
}

static bool is_executable(char const* path) {
	struct stat sb;

	if (stat(path, &sb) < 0) {
		return false;
	}

	if (S_ISDIR(sb.st_mode)) {
		return false;
	}

	return access(path, X_OK) == 0;
}

static char* find_bin(char const* cmd) {
	if (is_executable(cmd)) {
		return strdup(cmd);
	}

	// Look for binary in $PATH.
	// $PATH should be traversed from beginning to end (earlier entries have higher priority).

	char* const path = getenv("PATH");

	if (path == NULL) {
		LOG_ERROR("getenv(\"PATH\"): couldn't find '$PATH' (and binary '%s' was not found)", cmd);
		_exit(EXIT_FAILURE);
	}

	char* const STR_CLEANUP orig_search = strdup(path);
	assert(orig_search != NULL);
	char* search = orig_search;

	char* tok;

	while ((tok = strsep(&search, ":"))) {
		char* full_path = NULL;
		asprintf(&full_path, "%s/%s", tok, cmd);
		assert(full_path != NULL);

		if (is_executable(full_path)) {
			return full_path;
		}

		free(full_path);
	}

	LOG_ERROR("Couldn't find binary '%s' in '$PATH'.", cmd);
	return NULL;
}

bool cmd_exists(char const* cmd) {
	char* const STR_CLEANUP path = find_bin(cmd);
	return path != NULL;
}

int cmd_exec_inplace(cmd_t* cmd) {
	assert(cmd->len > 1);
	char* const STR_CLEANUP path = find_bin(cmd->args[0]);

	if (path == NULL) {
		return -1;
	}

	return execv(path, cmd->args);
}

pid_t cmd_exec_async(cmd_t* cmd) {
	// Find binary.

	char* const STR_CLEANUP path = find_bin(cmd->args[0]);

	if (path == NULL) {
		return -1;
	}

	// Create stdout+stderr pipe.

	if (cmd->redirect) {
		int fd[2];

		if (pipe(fd) < 0) {
			LOG_FATAL("pipe: %s", strerror(errno));
			return -1;
		}

		cmd->in = fd[1];
		cmd->out = fd[0];
	}

	// Create stdin pipe.

	if (cmd->pending_stdin != NULL) {
		int fd[2];

		if (pipe(fd) < 0) {
			LOG_FATAL("pipe: %s", strerror(errno));
			return -1;
		}

		cmd->stdin_in = fd[1];
		cmd->stdin_out = fd[0];
	}

	// Spawn process.
	// We can't use 'fork()' here, because we could be called from a multi-threaded context.
	// https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.getting_started/topic/s1_procs_Multithreaded_fork.html
	// I used the excellent accepted answer here to figure out the piping stuff:
	// https://stackoverflow.com/questions/13893085/posix-spawnp-and-piping-child-output-to-a-string

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);

	if (cmd->redirect) {
		posix_spawn_file_actions_addclose(&actions, cmd->out);

		posix_spawn_file_actions_adddup2(&actions, cmd->in, STDOUT_FILENO);
		posix_spawn_file_actions_adddup2(&actions, cmd->in, STDERR_FILENO);
	}

	if (cmd->pending_stdin != NULL) {
		posix_spawn_file_actions_adddup2(&actions, cmd->stdin_out, STDIN_FILENO);
		posix_spawn_file_actions_addclose(&actions, cmd->stdin_in);
	}

	extern char** environ;
	pid_t pid;

	if (posix_spawnp(&pid, path, &actions, NULL, cmd->args, environ) < 0) {
		LOG_ERROR("posix_spawnp: %s", strerror(errno));
		pid = -1;

		if (cmd->redirect) {
			close(cmd->out);
			cmd->out = -1;
		}

		close(cmd->stdin_in);
		cmd->stdin_in = -1;
	}

	posix_spawn_file_actions_destroy(&actions);

	if (cmd->redirect) {
		close(cmd->in);
		cmd->in = -1;
	}

	// stdin stuff.

	close(cmd->stdin_out);
	cmd->stdin_out = -1;

	if (cmd->pending_stdin != NULL) {
		write(cmd->stdin_in, cmd->pending_stdin, strlen(cmd->pending_stdin) + 1);
		close(cmd->stdin_in);
		cmd->stdin_in = -1;
	}

	return pid;
}

static int wait_for_process(pid_t pid, int* sig) {
	assert(sig != NULL);

	int wstatus = 0;
	while (waitpid(pid, &wstatus, 0) > 0)
		;

	if (WIFSIGNALED(wstatus)) {
		*sig = WTERMSIG(wstatus);
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

	cmd->sig = 0;

	free(cmd->out_buf);
	cmd->out_buf = strdup("");
	assert(cmd->out_buf != NULL);

	if (!cmd->redirect) {
		goto just_wait;
	}

	// If redirecting output, continuously read it.
	// This is to prevent the process blocking if it has filled its pipe buffer.

	int const pipe = cmd->out;

	if (pipe == -1) {
		goto just_wait;
	}

	size_t total = 0;

	char chunk[4096];
	ssize_t bytes;

	while ((bytes = read(pipe, chunk, sizeof chunk)) > 0) {
		total += bytes;

		cmd->out_buf = realloc(cmd->out_buf, total + 1);
		assert(cmd->out_buf != NULL);
		memcpy(cmd->out_buf + total - bytes, chunk, bytes);
	}

	if (bytes < 0) {
		LOG_WARN("%s: Failed to read from %d: %s", __func__, pipe, strerror(errno));
	}

	cmd->out_buf[total] = '\0';

just_wait:

	cmd->rv = wait_for_process(pid, &cmd->sig);
	return cmd->rv;
}

char* cmd_read_out(cmd_t* cmd) {
	if (cmd->sig == 0) {
		return cmd->out_buf;
	}

	// If we terminated due to a signal, append that to the output.

	char* STR_CLEANUP sig_str = NULL;
	asprintf(&sig_str, BOLD RED "Terminated by signal: %s\n", strsignal(cmd->sig));
	assert(sig_str != NULL);
	size_t const sig_len = strlen(sig_str);

	size_t total = strlen(cmd->out_buf);

	cmd->out_buf = realloc(cmd->out_buf, total + sig_len + 1);
	assert(cmd->out_buf != NULL);
	memcpy(cmd->out_buf + total, sig_str, sig_len);
	total += sig_len;

	cmd->out_buf[total] = '\0';
	return cmd->out_buf;
}

void cmd_log(cmd_t* cmd, char const* cookie, char const* prefix, char const* infinitive, char const* past, bool log_success) {
	char* const out = cmd_read_out(cmd);
	bool const is_out = out[0] != '\0';
	bool const log_out = is_out && (log_success || cmd->rv < 0);
	char* const suffix = log_out ? ":" : ".";

#define P prefix ? prefix : "", prefix ? ": " : ""

	if (cmd->rv < 0) {
		LOG_ERROR("%s" CLEAR "%sFailed to %s%s", P, infinitive, suffix);
	}

	else {
		LOG_SUCCESS("%s" CLEAR "%sSuccessfully %s%s", P, past, suffix);
	}

#undef P

	if (log_out) {
		printf("%s", out);
	}

	// Write out log to file, only if there is one.
	// If not, attempt to remove the existing one anyway.

	if (cookie == NULL) {
		return;
	}

	char* STR_CLEANUP path;
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
	set_owner(path);
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

	free(cmd->args);
	cmd->len = 0;
	cmd->args = NULL;

	free(cmd->out_buf);
	free(cmd->pending_stdin);

	if (cmd->in >= 0) {
		close(cmd->in);
	}

	if (cmd->out >= 0) {
		close(cmd->out);
	}

	if (cmd->stdin_in >= 0) {
		close(cmd->stdin_in);
	}

	if (cmd->stdin_out >= 0) {
		close(cmd->stdin_out);
	}
}
