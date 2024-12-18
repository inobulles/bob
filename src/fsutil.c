// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int rm(char const* path, char** err) {
	// Same comment as for 'copy'.

	cmd_t cmd;
	cmd_create(&cmd, "rm", "-rf", path, NULL);

	int const rv = cmd_exec(&cmd);

	if (rv < 0 && *err != NULL) {
		*err = cmd_read_out(&cmd);
	}

	cmd_free(&cmd);
	return rv;
}

int copy(char const* src, char const* dst, char** err) {
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

	if (is_dir && rm(dst, err) < 0) {
		return -1;
	}

	// Finally, actually run the copy command.

	cmd_t cmd;
	cmd_create(&cmd, "cp", "-RP", src, dst, NULL);

	int const rv = cmd_exec(&cmd);

	if (rv < 0 && err != NULL) {
		*err = cmd_read_out(&cmd);
		size_t const len = strlen(*err);

		if (len >= 1) {
			(*err)[len - 1] = '\0';
		}
	}

	cmd_free(&cmd);
	return rv;
}

extern bool running_as_root;
extern uid_t owner;

int set_owner(char const* path) {
	// We only want to lower permissions if we're running as root.

	if (!running_as_root) {
		return 0;
	}

	// If the absolute output path hasn't yet been set, this is being called as a means to ensure the output path exists.
	// That being the case, we can skip to the chown call.

	char* CLEANUP_STR full_path = NULL;

	if (abs_out_path == NULL) {
		goto chown;
	}

	// We only want to mess with the permissions of stuff in the output directory (.bob).

	full_path = realpath(path, NULL);

	if (full_path == NULL) {
		assert(errno != ENOMEM);
		LOG_WARN("realpath(\"%s\"): %s", path, strerror(errno));
		return -1;
	}

	if (strstr(full_path, abs_out_path) != full_path) {
		return 0;
	}

	// If those checks all pass, we can finally set the owner of the file.

chown:

	if (chown(path, owner, -1) < 0) {
		LOG_WARN("chown(\"%s\"): %s", path, strerror(errno));
		return -1;
	}

	return 0;
}

int mkdir_wrapped(char const* path, mode_t mode) {
	int const rv = mkdir(path, mode);

	if (rv < 0 && errno == EEXIST) {
		return 0;
	}

	else if (rv == 0) {
		set_owner(path);
		return 0;
	}

	return rv;
}
