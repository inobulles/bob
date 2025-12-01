// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2025 Aymeric Wibo

#include <common.h>

#include <cmd.h>
#include <deps.h>
#include <fsutil.h>
#include <logging.h>
#include <ncpu.h>
#include <pool.h>

#include <assert.h>
#include <sys/param.h>

static pthread_mutex_t logging_lock = PTHREAD_MUTEX_INITIALIZER;

static bool build_task(void* data) {
	dep_node_t* const dep = data;

	// TODO If dep->kind == DEP_KIND_GIT, we should not be rebuilding this at all so long as a relative $out_path (I think?) directory exists (though what happens if we need to reinstall?).

	// Log that we're building.

	char const* human = dep->human;

	if (human == NULL) {
		human = strrchr(dep->path, '/');

		if (human++ == NULL) {
			human = dep->path;
		}
	}

	pthread_mutex_lock(&logging_lock);
	LOG_INFO("%s" CLEAR ": Building dependency...", human);
	pthread_mutex_unlock(&logging_lock);

	// Actually build the dependency.
	// We shouldn't pass -o here because the dependency should be built in its own output path.
	// XXX If the dependency wants to use a different output path, we should probably add a function in the Bob build script to set the output path to something else instead of having an -o switch.
	// Since we're just building at the moment (no installing), only pass -t, not -p.

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, init_name, "-D", "-p", install_prefix, "-C", NULL);
	cmd_addf(&cmd, "%s/%s", dep->path, dep->build_path);

	bool dep_own_prefix;
	assert(would_set_owner(install_prefix, &dep_own_prefix) == 0);

	if (dep_own_prefix) {
		cmd_add(&cmd, "-O");
	}

	cmd_add(&cmd, "install");
	int const rv = cmd_exec(&cmd);

	pthread_mutex_lock(&logging_lock);
	cmd_log(&cmd, NULL, human, "build dependency", "built dependency", false);
	pthread_mutex_unlock(&logging_lock);

	return rv < 0;
}

/**
 * Reset the built_deps flag for all nodes in the tree.
 *
 * @param tree The root of the dependency tree.
 * @return The total number of descendant nodes in the (sub-)tree, root node included.
 */
static size_t reset_built_deps(dep_node_t* tree) {
	size_t total_node_count = 1;

	for (size_t i = 0; i < tree->child_count; i++) {
		dep_node_t* const child = &tree->children[i];
		total_node_count += reset_built_deps(child);
	}

	tree->built_deps = tree->child_count == 0; // I.e. is a leaf in the unbuilt tree.
	return total_node_count;
}

/**
 * Traverse the tree and get the next batch of leaves to build from the dependency tree.
 *
 * Nodes which have the built_deps flag are considered to have been removed from the tree for this operation.
 *
 * @param tree The root of the dependency tree.
 * @param leaves The array of leaves to fill.
 * @param leaf_count Pointer whose value is set to the number of leaves in the array.
 * @param already_built The array of already built dependencies. This used by this function to know which dependencies have already been built and to avoid adding them to the leaves array. This function will also add the leaves of the batch to this array.
 * @param already_built_count Pointer whose value is set to the number of already built dependencies in the array.
 * @return true if the tree is empty (i.e., all nodes have been built), false otherwise. This is used to stop the recursion.
 */
static bool next_batch(
	dep_node_t* tree,
	dep_node_t** leaves,
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
				return true;
			}
		}

		leaves[(*leaf_count)++] = tree;
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

	size_t const total_node_count = reset_built_deps(tree);

	char* already_built[total_node_count + 1]; // XXX +1 because zero-length VLA is UB.
	size_t already_built_count = 0;

	for (;;) {
		dep_node_t* leaves[total_node_count + 1]; // XXX +1 because zero-length VLA is UB.
		size_t leaf_count = 0;

		next_batch(tree, leaves, &leaf_count, already_built, &already_built_count);
		assert(leaf_count <= total_node_count);

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
