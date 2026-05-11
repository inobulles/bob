// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#include <common.h>

#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void check_gitignore(void) {
	// Only relevant in a git repo.

	if (access(".git", F_OK) != 0) {
		return;
	}

	FILE* const f = fopen(".gitignore", "r");

	if (f == NULL) {
		LOG_WARN("No .gitignore found. Consider creating it with \"%s\" in it.", targetless_out_path);
		return;
	}

	char* STR_CLEANUP real_out_path = realerpath(targetless_out_path);
	assert(real_out_path != NULL);

	char* STR_CLEANUP line = NULL; // getline(3) is weird but... trust this is correct, and we don't needa realloc or anything.
	size_t len = 0;
	bool found = false;
	size_t line_count = 0;

	while (getline(&line, &len, f) != -1) {
		// Done because I'm scared of the perf implications of this.

		if (++line_count > 1000) {
			LOG_WARN("what's wrong with you?");
			break;
		}

		line[strlen(line) - 1] = '\0'; // Remove newline that getline(3) keeps for some reason.

		char* STR_CLEANUP real_gitignore_path = realerpath(line);

		if (real_gitignore_path == NULL) {
			continue;
		}

		if (strcmp(real_out_path, real_gitignore_path) == 0) {
			found = true;
			break;
		}
	}

	fclose(f);

	if (!found) {
		LOG_WARN("\"%s\" is not in .gitignore. Consider adding it.", targetless_out_path);
	}
}
