// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <frugal.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
