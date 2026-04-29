// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Aymeric Wibo

#include <common.h>

#include <cookie.h>
#include <frugal.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

bool frugal_flags(flamingo_val_t* flags, char* out) {
	char path[strlen(out) + 7];
	snprintf(path, sizeof path, "%s.flags", out);

	// Turn flags into string.

	assert(flags->kind == FLAMINGO_VAL_KIND_VEC);

	char* STR_CLEANUP flag_str = calloc(1, 1);
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
	char* STR_CLEANUP prev_flag_str = NULL;

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

	// Note that we want to compare the order of these flags too, so just compare the strings.
	// There could be some cases where flipping the order of flags could change the build, which is perhaps rare but let's not try to be too smart here.

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
	set_owner(path);

	return true;
}

int frugal_mtime(
	bool* do_work,
	char const log_prefix[static 1],
	size_t dep_count,
	char* const* deps,
	char* target
) {
	assert(log_prefix != NULL);
	*do_work = true; // When in doubt, do the work.

	// If target file doesn't exist yet, we need to do work.

	struct stat target_sb;

	if (stat(target, &target_sb) < 0) {
		if (errno != ENOENT) {
			LOG_FATAL("%s: Failed to stat target '%s': %s", log_prefix, target, strerror(errno));
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
			LOG_FATAL("%s: Failed to stat dependency '%s': %s", log_prefix, dep, strerror(errno));
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

int frugal_link(
	bool* do_link,
	char const log_prefix[static 1],
	flamingo_val_t* flags,
	size_t obj_count,
	char** objs,
	char* out
) {
	*do_link = true;

	// Re-link if flags have changed.

	if (frugal_flags(flags, out)) {
		return 0;
	}

	// Re-link if any statically linked dependencies have changed.
	// We know we have a static dependency when there's a cookie in the flags.

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];

		if (has_built_cookie(flag->str.str, flag->str.size)) {
			return 0;
		}
	}

	// Now, this is a little more involved.
	// We're going to want to look through all library search paths and -l flags to look for static libraries which might have changed.
	// Collect lib search paths.

	size_t search_path_count = 1;
	char** search_paths = malloc(search_path_count + sizeof *search_paths);
	assert(search_paths != NULL);

	// First lib search path is just /lib in the install prefix.

	asprintf(&search_paths[0], "%s/lib", install_prefix);
	assert(search_paths[0] != NULL);

	// Next lib search paths are all the -L flags.

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* flag = flags->vec.elems[i];
		size_t flen = flag->str.size;
		char const* fstr = flag->str.str;

		if (strncmp(fstr, "-L", 2) != 0) {
			continue;
		}

		search_paths = realloc(search_paths, (search_path_count + 1) * sizeof *search_paths);
		assert(search_paths != NULL);

		char* search_path;

		if (flen == 2) { // If we only have -L, then go to next element in vector.
			if (i >= flags->vec.count - 1) {
				LOG_FATAL("%s: No search path provided after -L", log_prefix);
				return -1;
			}

			flag = flags->vec.elems[i + 1];
			search_path = strndup(flag->str.str, flag->str.size);
		} else {
			assert(flen > 2);
			search_path = strndup(fstr + 2, flen - 2);
			assert(search_path != NULL);
		}

		if (access(search_path, F_OK) != 0) {
			LOG_FATAL("%s: Failed to access library search path '%s': %s", log_prefix, search_path, strerror(errno));
			return -1;
		}

		search_paths[search_path_count++] = search_path;
	}

	// Resolve -lXXX (→ libXXX.a) and -l:filename (→ exact name) to real paths.

	size_t extra_count = 0;
	char** extra = NULL;

	for (size_t i = 0; i < flags->vec.count; i++) {
		flamingo_val_t* const flag = flags->vec.elems[i];
		size_t const flen = flag->str.size;
		char const* const fstr = flag->str.str;

		if (strncmp(fstr, "-l", 2) != 0) {
			continue;
		}

		bool const exact = flen > 3 && fstr[2] == ':';
		char const* const name = fstr + (exact ? 3 : 2);
		size_t const nlen = flen - (exact ? 3 : 2);

		// This is the correct order for processing search paths.

		for (size_t j = 0; j < search_path_count; j++) {
			char* path = NULL;

			if (exact) {
				asprintf(&path, "%s/%.*s", search_paths[j], (int) nlen, name);
			} else {
				asprintf(&path, "%s/lib%.*s.a", search_paths[j], (int) nlen, name);
			}

			assert(path != NULL);

			if (access(path, F_OK) == 0) {
				extra = realloc(extra, (extra_count + 1) * sizeof *extra);
				assert(extra != NULL);
				extra[extra_count++] = path;
				break;
			}

			free(path);
		}
	}

	// Free all our search paths as we don't need em no more.

	for (size_t i = 0; i < search_path_count; i++) {
		free(search_paths[i]);
	}

	free(search_paths);

	// Run mtime check with combined deps.

	size_t const total = obj_count + extra_count;
	char** deps = malloc(total * sizeof *deps);
	assert(deps != NULL);

	memcpy(deps, objs, obj_count * sizeof *objs);
	memcpy(deps + obj_count, extra, extra_count * sizeof *extra);

	int const rv = frugal_mtime(do_link, log_prefix, total, deps, out);

	free(deps);

	for (size_t i = 0; i < extra_count; i++) {
		free(extra[i]);
	}

	free(extra);

	return rv;
}
