// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <frugal.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

bool frugal_flags(flamingo_val_t* flags, char* out) {
	char path[strlen(out) + 7];
	snprintf(path, sizeof path, "%s.flags", out);

	// Turn flags into string.

	assert(flags->kind == FLAMINGO_VAL_KIND_VEC);

	char* CLEANUP_STR flag_str = calloc(1, 1);
	assert(flag_str != NULL);

	size_t size = 0;

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];
		assert(flag->kind == FLAMINGO_VAL_KIND_STR);

		size_t const extra = flag->str.size + 1;

		flag_str = realloc(flag_str, size + extra + 1);
		assert(flag_str != NULL);
		snprintf(flag_str + size, extra + 1, "%.*s\n", (int) flag->str.size, flag->str.str);
		size += extra;
	}

	// Read previous flags file.
	// If it doesn't exist, we immediately write out the flags and can't be frugal.
	// If they differ, we also write out the new flags.

	FILE* f = fopen(path, "rw");
	char* CLEANUP_STR prev_flag_str = NULL;

	if (f == NULL) {
		goto write_out;
	}

	fseek(f, 0, SEEK_END);
	size_t const prev_size = ftell(f);
	rewind(f);

	prev_flag_str = malloc(prev_size + 1);
	assert(prev_flag_str != NULL);
	fread(prev_flag_str, 1, prev_size, f);
	prev_flag_str[prev_size] = '\0';

	fclose(f);

	if (strcmp(prev_flag_str, flag_str) != 0) {
		goto write_out;
	}

	return false;

	// Write out flags if they differ.

write_out:

	f = fopen(path, "w");

	if (f == NULL) {
		LOG_WARN("Failed to open '%s' for writing.", path);
		return true;
	}

	fprintf(f, "%s", flag_str);
	fclose(f);

	return true;
}

int frugal_mtime(bool* do_work, char const prefix[static 1], size_t dep_count, char* const* deps, char* target) {
	assert(prefix != NULL);
	*do_work = true; // When in doubt, do the work.

	// If target file doesn't exist yet, we need to do work.

	struct stat target_sb;

	if (stat(target, &target_sb) < 0) {
		if (errno != ENOENT) {
			LOG_FATAL("%s: Failed to stat target '%s': %s", prefix, target, strerror(errno));
			return -1;
		}

		*do_work = true;
		return 0;
	}

	// If any dependency is newer than target, we need to do work.
	// TODO There is a case where we could build, modify, and build again in the space of one minute in which case changes won't be reflected, but that's such a small edgecase I don't think it's worth letting the complexity spirit demon enter.
	// TODO Another thing to consider is that I'm not sure if a moved file also updates its modification timestamp (i.e. src/main.c is updated by 'mv src/{other,main}.c').

	for (size_t i = 0; i < dep_count; i++) {
		char* const dep = deps[i];
		struct stat dep_sb;

		if (stat(dep, &dep_sb) < 0) {
			LOG_FATAL("%s: Failed to stat dependency '%s': %s", prefix, dep, strerror(errno));
			return -1;
		}

		// Strict comparison because if b is built right after a, we don't want to rebuild b d'office.

		if (dep_sb.st_mtime > target_sb.st_mtime) {
			*do_work = true;
			return 0;
		}
	}

	// All conditions passed not to do work.

	*do_work = false;
	return 0;
}
