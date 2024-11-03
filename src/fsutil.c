// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int rm(char const* path) {
	// Same comment as for 'copy'.

	cmd_t cmd;
	cmd_create(&cmd, "rm", "-rf", path, NULL);

	int const rv = cmd_exec(&cmd);

	if (rv < 0) {
		char* CLEANUP_STR out = cmd_read_out(&cmd);
		printf("%s", out);
	}

	cmd_free(&cmd);
	return rv;
}

int copy(char const* src, char const* dst) {
	// It's unfortunate, but to be as cross-platform as possible, we must shell out execution to the 'cp' binary.
	// Would've loved to use libcopyfile but, alas, POSIX is missing features :(

	// If 'src' is a directory, append a slash to it to override stupid cp(1) behaviour.

	struct stat sb;

	if (stat(src, &sb) < 0) {
		LOG_ERROR("stat(\"%s\"): %s", src, strerror(errno));
		return -1;
	}

	bool const is_dir = S_ISDIR(sb.st_mode);
	char* CLEANUP_STR dir_src = NULL;

	if (is_dir) {
		asprintf(&dir_src, "%s/", src);
		assert(dir_src != NULL);
		src = dir_src;
	}

	// Also, if 'src' is a directory, we'd like to start off by recursively removing it.
	// This is so the copy overwrites the destination rather than preserving the original files.

	if (is_dir && rm(dst) < 0) {
		return -1;
	}

	// Finally, actually run the copy command.

	cmd_t cmd;
	cmd_create(&cmd, "cp", "-RpP", src, dst, NULL);

	int const rv = cmd_exec(&cmd);

	if (rv < 0) {
		char* CLEANUP_STR out = cmd_read_out(&cmd);
		printf("%s", out);
	}

	cmd_free(&cmd);
	return 0;
}
