// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <util.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

int wait_for_process(pid_t pid) {
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

pid_t execute_async(exec_args_t* self) {
	char** const exec_args = self->args;

	if (pipe_create(&self->pipe) < 0) {
		return -1;
	}

	// fork process

	pid_t pid = fork();

	if (pid < 0) {
		LOG_ERROR("fork: %s", strerror(errno))
		return -1;
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
			char* CLEANUP_STR path = NULL;
			if (asprintf(&path, "%s/%s", tok, query)) {}

			exec_args[0] = path;

			if (!execv(exec_args[0], exec_args)) {
				_exit(EXIT_SUCCESS);
			}
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
	pid_t const pid = execute_async(_exec_args);

	if (pid < 0) {
		return EXIT_FAILURE;
	}

	return wait_for_process(pid);
}
