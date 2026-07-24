// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#include <common.h>

#include <alloc.h>
#include <artifact.h>
#include <frugal.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <sys/stat.h>

static flamingo_val_t* artifact_map = NULL;

int setup_artifact_map(flamingo_t* flamingo) {
	// Find the artifact map.

	flamingo_scope_t* const scope = flamingo->env->scope_stack[0];

	artifact_map = NULL;
	flamingo_var_t* map = NULL;

	for (size_t i = 0; i < scope->vars_size; i++) {
		map = &scope->vars[i];

		if (flamingo_cstrcmp(map->key, "artifacts", map->key_size) != 0) {
			continue;
		}

		if (map->val->kind == FLAMINGO_VAL_KIND_NONE) {
			return 0;
		}

		if (map->val->kind != FLAMINGO_VAL_KIND_MAP) {
			LOG_FATAL("Artifact map must be a map.");
			return -1;
		}

		goto found;
	}

	LOG_FATAL("Artifact map was never declared." PLZ_REPORT);
	return -1;

found:

	if (map->val->map.count == 0) {
		LOG_WARN("Artifact map is empty; nothing to output! You may remove it.");
		return 0;
	}

	// Validate artifact map entries.

	for (size_t i = 0; i < map->val->map.count; i++) {
		flamingo_val_t* const key_val = map->val->map.keys[i];
		flamingo_val_t* const val_val = map->val->map.vals[i];

		if (key_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Artifact map key must be a string.");
			return -1;
		}

		if (val_val->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Artifact map value must be a string.");
			return -1;
		}
	}

	artifact_map = map->val;
	return 0;
}

static int create_artifact(flamingo_val_t* key_val, char* artifact_path) {
	// Get absolute path of source.

	char* const STR_CLEANUP key = strndup_c(key_val->str.str, key_val->str.size);
	char* const STR_CLEANUP path = realerpath(key);

	if (path == NULL) {
		LOG_FATAL("Couldn't find source file (from artifact map): %s", key);
		return -1;
	}

	struct stat sb;

	if (stat(path, &sb) < 0) {
		LOG_FATAL("Couldn't stat source file (from artifact map): %s", key);
		return -1;
	}

	if (!S_ISREG(sb.st_mode)) {
		LOG_FATAL("Source file (from artifact map) is not a regular file: %s", key);
		return -1;
	}

	// Check modification times.

	bool do_artifact_copy = false;

	if (frugal_mtime(&do_artifact_copy, "artifact copy", 1, &key, artifact_path) < 0) {
		return -1;
	}

	if (!do_artifact_copy) {
		LOG_SUCCESS("%s" CLEAR ": Artifact already copied.", artifact_path);
		return 0;
	}

	// Actually copy over files.

	LOG_INFO("%s" CLEAR ": Copying artifact from '%s'...", artifact_path, key);

	char* STR_CLEANUP err = NULL;

	if (copy(key, artifact_path, &err) < 0) {
		LOG_FATAL("Failed to copy '%s' to '%s': %s", key, artifact_path, err);
		return -1;
	}

	set_owner(artifact_path);
	LOG_SUCCESS("%s" CLEAR ": Successfully copied artifact.", artifact_path);

	return 0;
}

int create_artifacts(void) {
	if (artifact_map == NULL) {
		return 0;
	}

	for (size_t i = 0; i < artifact_map->map.count; i++) {
		flamingo_val_t* const val_val = artifact_map->map.vals[i];
		char* const val = strndup_c(val_val->str.str, val_val->str.size);

		if (create_artifact(artifact_map->map.keys[i], val) < 0) {
			return -1;
		}
	}

	return 0;
}
