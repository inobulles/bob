// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"

#include "../env.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static int import(flamingo_t* flamingo, char* path) {
	// TODO This is really relative to the caller, not the current file.
	// Should it be relative to the current file though?
	// If we do it like that, then a.b.c wouldn't be able to import a.b for example.
	// Maybe we should do it relative to the first file that was parsed, and then relative to the import path when importing non-relatively.

	int rv = 0;

	// Read file.

	struct stat sb;

	if (stat(path, &sb) < 0) {
		rv = error(flamingo, "failed to import '%s': stat: %s", path, strerror(errno));
		goto err_stat;
	}

	size_t const src_size = sb.st_size;

	FILE* const f = fopen(path, "r");

	if (f == NULL) {
		rv = error(flamingo, "failed to import '%s': fopen: %s", path, strerror(errno));
		goto err_fopen;
	}

	char* const src = malloc(src_size);
	assert(src != NULL);

	if (fread(src, 1, src_size, f) != src_size) {
		rv = error(flamingo, "failed to import '%s': fread: %s", path, strerror(errno));
		goto err_fread;
	}

	// Allocate space for a new flamingo engine and its source on the current instance.

	flamingo->import_count++;

	flamingo->imported_flamingos = realloc(flamingo->imported_flamingos, flamingo->import_count * sizeof *flamingo->imported_flamingos);
	assert(flamingo->imported_flamingos != NULL);

	flamingo->imported_srcs = realloc(flamingo->imported_srcs, flamingo->import_count * sizeof *flamingo->imported_srcs);
	assert(flamingo->imported_srcs != NULL);

	flamingo->imported_srcs[flamingo->import_count - 1] = src;
	flamingo_t* const imported_flamingo = &flamingo->imported_flamingos[flamingo->import_count - 1];

	// Create new flamingo engine.

	if (flamingo_create(imported_flamingo, flamingo->progname, src, src_size) < 0) {
		rv = error(flamingo, "failed to import '%s': flamingo_create: %s", path, strerror(errno));
		goto err_flamingo_create;
	}

	flamingo_register_external_fn_cb(imported_flamingo, flamingo->external_fn_cb, flamingo->external_fn_cb_data);
	flamingo_register_class_decl_cb(imported_flamingo, flamingo->class_decl_cb, flamingo->class_decl_cb_data);
	flamingo_register_class_inst_cb(imported_flamingo, flamingo->class_inst_cb, flamingo->class_inst_cb_data);

	// Set the scope stack for the imported flamingo instance to be the same as ours.

	if (flamingo_inherit_env(imported_flamingo, flamingo->env) < 0) {
		rv = error(flamingo, "failed to import '%s': flamingo_inherit_env: %s", path, flamingo_err(imported_flamingo));
		goto err_flamingo_inherit_scope_stack;
	}

	// Run the imported program.

	if (flamingo_run(imported_flamingo) < 0) {
		rv = error(flamingo, "failed to import '%s': flamingo_run: %s", path, flamingo_err(imported_flamingo));
		goto err_flamingo_run;
	}

	// Don't forget to copy back the scope stack, as after a few reallocs the pointers might be different!

	assert(flamingo->env->scope_stack_size == imported_flamingo->env->scope_stack_size);
	flamingo->env = imported_flamingo->env;

err_flamingo_run:
err_flamingo_inherit_scope_stack:
err_flamingo_create:
err_fread:

	fclose(f);

err_fopen:
err_stat:

	return rv;
}

static int parse_import_path(flamingo_t* flamingo, TSNode node, char** path_ref, size_t* path_len_ref) {
	assert(strcmp(ts_node_type(node), "import_path") == 0);
	assert(ts_node_child_count(node) == 1 || ts_node_child_count(node) == 3);

	// Get current path component (bit).

	TSNode const bit_node = ts_node_child_by_field_name(node, "bit", 3);
	char const* const bit_type = ts_node_type(bit_node);

	if (strcmp(bit_type, "identifier") != 0) {
		return error(flamingo, "expected identifier for bit, got %s", bit_type);
	}

	// Get the rest of the path.

	TSNode const rest_node = ts_node_child_by_field_name(node, "rest", 4);
	bool const has_rest = !ts_node_is_null(rest_node);

	if (has_rest) {
		char const* const rest_type = ts_node_type(rest_node);

		if (strcmp(rest_type, "import_path") != 0) {
			return error(flamingo, "expected import_path for rest, got %s", rest_type);
		}
	}

	// Get the actual path component and add it to our path accumulator.

	size_t const start = ts_node_start_byte(bit_node);
	size_t const end = ts_node_end_byte(bit_node);

	char const* const bit = flamingo->src + start;
	size_t const size = end - start;

	*path_ref = realloc(*path_ref, *path_len_ref + size + 1);

	if (*path_ref == NULL) {
		free(*path_ref);
		return error(flamingo, "failed to allocate memory for import path");
	}

	memcpy(*path_ref + *path_len_ref, bit, size);
	*path_len_ref += size + 1;
	(*path_ref)[*path_len_ref - 1] = '/';

	// Recursively parse the rest of the path.

	if (has_rest && parse_import_path(flamingo, rest_node, path_ref, path_len_ref) < 0) {
		return -1;
	}

	return 0;
}

static int parse_import(flamingo_t* flamingo, TSNode node) {
	assert(strcmp(ts_node_type(node), "import") == 0);

	size_t const child_count = ts_node_child_count(node);
	assert(child_count == 2 || child_count == 3);

	// Is the import relative to the current file?
	// If so, it means we need to follow the import file relative to the current one.
	// Otherwise, we'll have to iterate through the import paths to find it.

	TSNode const relative_node = ts_node_child_by_field_name(node, "relative", 8);
	bool const is_relative = !ts_node_is_null(relative_node);

	// Get the path of the import.

	TSNode const path_node = ts_node_child_by_field_name(node, "path", 4);
	char const* const path_type = ts_node_type(path_node);

	if (strcmp(path_type, "import_path") != 0) {
		return error(flamingo, "expected import_path for path, got %s", path_type);
	}

	// Parse the import path into an actual string path we can use.

	char* path = NULL;
	size_t path_len = 0;

	if (parse_import_path(flamingo, path_node, &path, &path_len) < 0) {
		free(path);
		return -1;
	}

	for (size_t i = 0; i < path_len; i++) {
		if (path[i] == '\0') {
			free(path);
			return error(flamingo, "one of the import path components contains a null byte somehow");
		}
	}

	char* import_path = NULL;
	int rv = asprintf(&import_path, "%.*s.fl", (int) path_len - 1, path);
	free(path);

	if (rv < 0 || import_path == NULL) {
		assert(rv < 0 && import_path == NULL);
		return error(flamingo, "failed to allocate memory for import path");
	}

	// If is a global import, go through the import paths until we find the file.

	if (!is_relative) {
		for (size_t i = 0; i < flamingo->import_path_count; i++) {
			char* full_path = NULL;
			asprintf(&full_path, "%s/%s", flamingo->import_paths[i], import_path);
			assert(full_path != NULL);

			if (access(full_path, F_OK) < 0) {
				free(full_path);
				continue;
			}

			free(import_path);
			import_path = full_path;

			break;
		}
	}

	// Actually import.

	rv = import(flamingo, import_path);
	free(import_path);

	return rv;
}
