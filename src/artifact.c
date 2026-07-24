// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#include <common.h>

#include <artifact.h>
#include <logging.h>

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
