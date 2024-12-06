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
	// We wanna explicitly duplicate an empty string if no serialized string is passed instead of just relying on the 'realloc' call, because the node could very well not have any children.

	if (serialized == NULL) {
		serialized = strdup("");
		assert(serialized != NULL);
	}

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

	serialized[depth + len] = '\n';
	serialized[depth + len + 1] = '\0';

	return add_children(serialized, node, depth + 1);
}

char* dep_node_serialize(dep_node_t* node) {
	assert(node->is_root == true);
	assert(node->path == NULL);

	return add_children(NULL, node, 0);
}

int dep_node_deserialize(dep_node_t* root, char* serialized) {
	root->is_root = true;
	root->path = NULL;

	root->child_count = 0;
	root->children = NULL;

	size_t stack_size = 1;
	dep_node_t** stack = malloc(stack_size * sizeof *stack);
	assert(stack != NULL);
	stack[0] = root;

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
		// We start at 1 for the same reason 'prev_depth' starts at 1; children of the root node have 0 'DEPTH_CHAR's before them.

		size_t depth = 1;

		for (size_t i = 0; i < strlen(tok); i++) {
			if (tok[i] != DEPTH_CHAR) {
				break;
			}

			depth++;
		}

		// If the depth is one level higher than the previous one, we are a child dependency to the previous one.
		// Then, we add the new dependency to our stack.

		if (depth == prev_depth + 1) {
			dep_node_t* const cur = stack[stack_size - 1];

			// Actually create the node object.

			cur->children = realloc(cur->children, (cur->child_count + 1) * sizeof *cur->children);
			assert(cur->children != NULL);
			dep_node_t* const node = &cur->children[cur->child_count++];

			node->path = strdup(tok);
			assert(node->path != NULL);

			node->child_count = 0;
			node->children = NULL;

			// Add a reference to this node to our stack.

			if (stack_size == 1) {
				stack = realloc(stack, (stack_size + 1) * sizeof *stack);
				assert(stack != NULL);
				stack[stack_size++] = node;
			}

			continue;
		}

		// A depth which is more than one level higher than the previous one is impossible.

		else if (depth > prev_depth + 1) {
			LOG_FATAL("Invalid depth in serialized dependency tree." PLZ_REPORT);
			return -1; // TODO Error label to free the deserialized dependency tree up until now. Also the stack.
		}

		// Any other depth means that we've rolled back.
		// Pop the extra stuff from the end of our stack.

		else {
			stack_size = depth + 1;
			continue;
		}

		prev_depth = depth;
	}

	free(stack);

	return 0;
}
