// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>
#include <deps.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEPTH_CHAR '\t'

static char* serialize_inner(dep_node_t* node, size_t depth);

static char* add_children(char* serialized, dep_node_t* node, size_t depth) {
	for (size_t i = 0; i < node->child_count; i++) {
		dep_node_t* const child = &node->children[i];
		char* const STR_CLEANUP serialized_child = serialize_inner(child, depth);

		size_t const prev_len = strlen(serialized);
		size_t const len = strlen(serialized_child);

		serialized = realloc(serialized, prev_len + len + 1);
		assert(serialized != NULL);

		memcpy(serialized + prev_len, serialized_child, len + 1);
	}

	return serialized;
}

static char* serialize_inner(dep_node_t* node, size_t depth) {
	size_t const len = strlen(node->path);

	char* const serialized = malloc(depth + len + 2);
	assert(serialized != NULL);

	memset(serialized, DEPTH_CHAR, depth);
	memcpy(serialized + depth, node->path, len);

	serialized[depth + len - 1] = '\n';
	serialized[depth + len] = '\0';

	return add_children(serialized, node, depth + 1);
}

char* dep_node_serialize(dep_node_t* node) {
	assert(node->is_root == true);
	assert(node->path == NULL);

	return add_children(NULL, node, 0);
}

// TODO Finish deserialization.

int dep_node_deserialize(dep_node_t* root, char* serialized) {
	root->is_root = true;
	root->path = NULL;

	root->child_count = 0;
	root->children = NULL;

	char* const STR_CLEANUP orig_backing = strdup(serialized);
	assert(orig_backing != NULL);

	char* backing = orig_backing;
	char* tok;

	size_t prev_depth = 0;

	while ((tok = strsep(&backing, "\n")) != NULL) {
		if (tok[0] == '\0') {
			continue;
		}

		// Find depth of node, and make sure that it makes sense.

		size_t depth = 0;

		for (size_t i = 0; i < strlen(tok); i++) {
			if (tok[i] != DEPTH_CHAR) {
				break;
			}

			depth++;
		}

		// If the depth is one level higher than the previous one, we are a child dependency to the previous one.

		if (depth == prev_depth + 1) {
			continue;
		}

		// A depth which is more than one level higher than the previous one is impossible.

		else if (depth > prev_depth + 1) {
			LOG_FATAL("Invalid depth in serialized dependency tree." PLZ_REPORT);
			return -1; // TODO Error label to free the deserialized dependency tree up until now.
		}

		// Any other depth means that we've rolled back.

		else {
			continue;
		}

		prev_depth = depth;

		// Then, actually create the node object.

		dep_node_t* const node = malloc(sizeof *node);
		assert(node != NULL);

		node->path = strdup(tok);
		assert(node->path != NULL);

		node->child_count = 0;
		node->children = NULL;
	}

	return 0;
}
