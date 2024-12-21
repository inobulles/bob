// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <cmd.h>
#include <deps.h>
#include <logging.h>
#include <ncpu.h>
#include <pool.h>

#include <assert.h>
#include <sys/param.h>

static pthread_mutex_t logging_lock = PTHREAD_MUTEX_INITIALIZER;

static bool build_task(void* data) {
	char const* const path = data;

	// Log that we're building.

	pthread_mutex_lock(&logging_lock);
	LOG_INFO("%s" CLEAR ": Building dependency...", path); // TODO Replace with human.
	pthread_mutex_unlock(&logging_lock);

	// Actually build the dependency.
	// We shouldn't pass -o here because the dependency should be built in its own output path.
	// XXX If the dependency wants to use a different output path, we should probably add a function in the Bob build script to set the output path to something else instead of having an -o switch.

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, init_name, "-p", install_prefix, "-C", path, NULL);

	cmd_add(&cmd, "-t");
	cmd_add(&cmd, tmp_install_prefix);

	cmd_add(&cmd, "build-no-deps");

	int const rv = cmd_exec(&cmd);

	pthread_mutex_lock(&logging_lock);
	cmd_log(&cmd, NULL, path, "build dependency", "built dependency", false);
	pthread_mutex_unlock(&logging_lock);

	return rv < 0;
}

static size_t reset_built_deps(dep_node_t* tree, size_t* max_child_count) {
	*max_child_count = MAX(*max_child_count, tree->child_count);
	size_t total_node_count = 1;

	for (size_t i = 0; i < tree->child_count; i++) {
		size_t const child_count = reset_built_deps(&tree->children[i], max_child_count);

		total_node_count += child_count;
		*max_child_count = MAX(*max_child_count, child_count);
	}

	tree->built_deps = !tree->child_count;
	return total_node_count;
}

static bool next_batch(
	dep_node_t* tree,
	char** leaves,
	size_t* leaf_count,
	char** already_built,
	size_t* already_built_count
) {
	if (tree->is_root) {
		goto is_root;
	}

	// Is this node a leaf? I.e., are we're interested in it for this batch?
	// If so, add it to the list of leaves, making sure it wasn't already built.

	if (tree->built_deps) {
		for (size_t i = 0; i < *already_built_count; i++) {
			if (strcmp(tree->path, already_built[i]) == 0) {
				return false;
			}
		}

		leaves[(*leaf_count)++] = tree->path;
		already_built[(*already_built_count)++] = tree->path;

		return true;
	}

	// If not, go down the tree and process the children.

is_root:;

	size_t children_built = 0;

	for (size_t i = 0; i < tree->child_count; i++) {
		children_built += next_batch(&tree->children[i], leaves, leaf_count, already_built, already_built_count);
	}

	if (children_built == tree->child_count) {
		tree->built_deps = true;
	}

	return false;
}

int deps_build(dep_node_t* tree) {
	// Walk through dependency tree and build each dependency, starting from the leaves.
	// To do this most efficiently, we start by popping off all the leaves and building them in parallel.
	// Once that's done, we pop off the next set of leaves, and so on, until we reach the root node (which this current Bob process is meant to build).

	size_t max_child_count = tree->child_count;
	size_t const total_child_count = reset_built_deps(tree, &max_child_count);

	char* already_built[total_child_count + 1]; // XXX +1 because zero-length VLA UB.
	size_t already_built_count = 0;

	for (;;) {
		char* leaves[max_child_count + 1]; // XXX +1 because zero-length VLA UB.
		size_t leaf_count = 0;

		next_batch(tree, leaves, &leaf_count, already_built, &already_built_count);
		assert(leaf_count <= max_child_count);

		if (leaf_count == 0) {
			break;
		}

		// Actually build the leaves in parallel.
		// TODO This can be majorly improved by not waiting for all leaves to be built before adding more stuff to the pool.

		pool_t pool;
		pool_init(&pool, ncpu());

		for (size_t i = 0; i < leaf_count; i++) {
			pool_add_task(&pool, build_task, leaves[i]);
		}

		if (pool_wait(&pool) < 0) {
			return -1;
		}
	}

	return 0;
}
