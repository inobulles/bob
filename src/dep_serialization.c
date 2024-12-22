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
	size_t const path_len = strlen(node->path);
	size_t const human_len = node->human == NULL ? 0 : strlen(node->human) + 1; // +1 to include the colon.

	size_t const size = human_len + path_len + 2;
	char* const serialized = malloc(depth + size);
	assert(serialized != NULL);
	memset(serialized, DEPTH_CHAR, depth);

	if (node->human == NULL) {
		snprintf(serialized + depth, size, "%s\n", node->path);
	}

	else {
		snprintf(serialized + depth, size, "%s:%s\n", node->human, node->path);
	}

	return add_children(serialized, node, depth + 1);
}

char* dep_node_serialize(dep_node_t* node) {
	assert(node->is_root == true);
	return add_children(NULL, node, 0);
}

static void free_stack(dep_node_t*** stack) {
	free(*stack);
}

int dep_node_deserialize(dep_node_t* root, char* serialized) {
	// Do all the necessary set up for the root node and the stack.

	root->is_root = true;
	root->path = NULL;
	root->human = NULL;

	root->child_count = 0;
	root->children = NULL;

	size_t stack_size = 1;
	dep_node_t** __attribute__((cleanup(free_stack))) stack = malloc(stack_size * sizeof *stack);
	assert(stack != NULL);
	stack[0] = root;

	// Figure out if we're using dependency tree tags or not.
	// If we are, jump to after that point in the string passed.
	// If we are not, assume the whole string is the meat of the dependency tree.

	char* const dep_tag_start = strstr(serialized, DEP_TAG_START);
	bool const use_tags = dep_tag_start != NULL;

	if (use_tags) {
		serialized = dep_tag_start + strlen(DEP_TAG_START);
	}

	// We need to keep a copy of the original backing string to free it later.
	// If using tags, remove all that is after the closing tag.

	char* const STR_CLEANUP orig_backing = strdup(serialized);
	assert(orig_backing != NULL);

	if (use_tags) {
		char* const dep_tag_end = strstr(orig_backing, DEP_TAG_END);

		if (dep_tag_end == NULL) {
			LOG_FATAL("Using dependency tree tags, but no closing dependency tree tag found." PLZ_REPORT);
			return -1;
		}

		dep_tag_end[0] = '\0';
	}

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

		// A depth which is more than one level higher than the previous one is impossible.

		if (depth > prev_depth + 1) {
			LOG_FATAL("Invalid depth in serialized dependency tree." PLZ_REPORT);

			deps_node_free(root);
			return -1;
		}

		// A less than or equal to depth means that we've rolled back.
		// Pop the extra stuff from the end of our stack.

		else if (depth <= prev_depth) {
			stack_size = depth;
		}

		dep_node_t* const cur = stack[stack_size - 1];

		// Actually create the node object.

		cur->children = realloc(cur->children, (cur->child_count + 1) * sizeof *cur->children);
		assert(cur->children != NULL);
		dep_node_t* const node = &cur->children[cur->child_count++];

		node->is_root = false;

		char const* const tuple = tok + depth - 1;
		char const* human = tuple;
		char const* path = strrchr(tuple, ':');

		if (path == NULL) {
			path = tuple;
			human = NULL;
		}

		else {
			assert(human != NULL);
			path++;

			node->human = strdup(human);
			assert(node->human != NULL);
			node->human[path - human - 1] = '\0';
		}

		node->path = strdup(path);
		assert(node->path != NULL);

		node->child_count = 0;
		node->children = NULL;

		// Add a reference to this node to our stack.

		stack = realloc(stack, (stack_size + 1) * sizeof *stack);
		assert(stack != NULL);
		stack[stack_size++] = node;

		prev_depth = depth;
	}

	return 0;
}
