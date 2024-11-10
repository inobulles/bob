// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

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

static char* find_bin(cmd_t* cmd) {
	if (is_executable(cmd->args[0])) {
		return strdup(cmd->args[0]);
	}

	// Look for binary in '$PATH'.

	char* const path = getenv("PATH");

	if (path == NULL) {
		LOG_ERROR("getenv(\"PATH\"): couldn't find '$PATH' (and binary '%s' was not found)", cmd->args[0]);
		_exit(EXIT_FAILURE);
	}

	char* const CLEANUP_STR orig_search = strdup(path);
	assert(orig_search != NULL);
	char* search = orig_search;

	char* tok;

	while ((tok = strsep(&search, ":"))) {
		char* full_path = NULL;
		asprintf(&full_path, "%s/%s", tok, cmd->args[0]);
		assert(full_path != NULL);

		if (is_executable(full_path)) {
			return full_path;
		}

		free(full_path);
	}

	LOG_ERROR("Couldn't find binary '%s' in '$PATH'", cmd->args[0]);
	return NULL;
}

int cmd_exec_inplace(cmd_t* cmd) {
	char* const CLEANUP_STR path = find_bin(cmd);

	if (path == NULL) {
		return -1;
	}

	return execv(path, cmd->args);
}

pid_t cmd_exec_async(cmd_t* cmd) {
	// Find binary.

	char* const CLEANUP_STR path = find_bin(cmd);

	if (path == NULL) {
		return -1;
	}

	// Create pipes.

	int fd[2];

	if (pipe(fd) < 0) {
		LOG_FATAL("pipe: %s", strerror(errno));
		return -1;
	}

	cmd->in = fd[1];
	cmd->out = fd[0];

	// Spawn process.
	// We can't use 'fork()' here, because we could be called from a multi-threaded context.
	// https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.getting_started/topic/s1_procs_Multithreaded_fork.html
	// I used the excellent accepted answer here to figure out the piping stuff:
	// https://stackoverflow.com/questions/13893085/posix-spawnp-and-piping-child-output-to-a-string

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);

	posix_spawn_file_actions_addclose(&actions, cmd->out);

	posix_spawn_file_actions_adddup2(&actions, cmd->in, STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, cmd->in, STDERR_FILENO);

	extern char** environ;
	pid_t pid;

	if (posix_spawnp(&pid, path, &actions, NULL, cmd->args, environ) < 0) {
		LOG_ERROR("posix_spawnp: %s", strerror(errno));
		pid = -1;

		close(cmd->out);
		cmd->out = -1;
	}

	posix_spawn_file_actions_destroy(&actions);

	close(cmd->in);
	cmd->in = -1;

	return pid;
}

static int wait_for_process(pid_t pid, int* _Nonnull sig) {
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
	cmd->rv = wait_for_process(pid, &cmd->sig);

	return cmd->rv;
}

char* cmd_read_out(cmd_t* cmd) {
	// Actually read pipe output.

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
		memcpy(out + total - bytes, chunk, bytes);
	}

	if (bytes < 0) {
		LOG_WARN("%s: Failed to read from %d: %s", __func__, pipe, strerror(errno));
	}

	// If we terminated due to a signal, append that to the output.

	if (cmd->sig != 0) {
		char* CLEANUP_STR sig_str = NULL;
		asprintf(&sig_str, BOLD RED "Terminated by signal: %s\n", strsignal(cmd->sig));
		assert(sig_str != NULL);
		size_t const sig_len = strlen(sig_str);

		out = realloc(out, total + sig_len + 1);
		assert(out != NULL);
		memcpy(out + total, sig_str, sig_len);
		total += sig_len;
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
