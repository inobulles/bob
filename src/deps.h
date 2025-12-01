// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2025 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

#define TAG_NAME "bob-dep-tree"

#define DEP_TAG_START "<" TAG_NAME ">\n"
#define DEP_TAG_END "</" TAG_NAME ">\n"

#define BOB_DEPS_CIRCULAR "<" TAG_NAME " circular />\n"

typedef enum {
	DEP_KIND_INVALID = 0, // Set to 0 explicitly, because we want strtoi failures to return DEP_KIND_INVALID.
	DEP_KIND_LOCAL,
	DEP_KIND_GIT,
} dep_kind_t;

typedef struct dep_node_t dep_node_t;

/**
 * Node (i.e. dependency) in the dependency tree.
 */
struct dep_node_t {
	/**
	 * Whether this node is the root dependency node.
	 */
	bool is_root;

	dep_kind_t kind;

	/**
	 * Path to actual dependency in Bob's dependency cache.
	 */
	char* path;

	/**
	 * Human-readable name for this dependency.
	 *
	 * E.g. for git repos, this is just the repo name, and for local dependencies this is the last component in the path.
	 */
	char* human;

	/**
	 * Path to change into to build the dependency.
	 *
	 * E.g. ZSTD's lib Meson script is at build/meson/meson.build, so you must cd to build/meson.
	 */
	char* build_path;

	/**
	 * The children of this node, i.e. all the dependencies it depends on.
	 */
	struct dep_node_t* children;
	size_t child_count;

	/**
	 * If all the dependencies of this node have already been built, this is set.
	 *
	 * This field is only used during the dependency build process.
	 */
	bool built_deps;
};

int deps_build(dep_node_t* tree);

// Dependency tree stuff.

/**
 * Ensure consistency of dependency tree.
 *
 * This is kind of a kitchen sink function, which:
 * - Makes sure all our direct dependencies are downloaded (and holds on to hash for later). See {@link ensure_deps_cache}.
 * - Maps out the dependency tree and makes sure it isn't circular (see {@link path_hashes}.
 * - If the hash computed earlier has not changed v. 'deps.hash' (previous hash), deserialize dependency tree cache and finish.
 * - Otherwise, if anything could not be found or has changed, rebuild the dependency tree and write it out along with its hash.
 *
 * @param deps_vec The Flamingo dependency vector in the Bob config.
 * @param path_len Length of the path of dependencies leading up to the one the current Bob process is running for, i.e. its parents, i.e. the callers.
 * @param path_hashes Path of dependency hashes leading up to the one the current Bob process is running for, i.e. its parents, i.e. the callers.
 * @param circular Set to true if tree was found to be circular, false if not.
 * @return The dependency tree.
 */
dep_node_t* get_deps_tree(flamingo_val_t* deps_vec, size_t path_len, uint64_t* path_hashes, bool* circular);
void deps_node_free(dep_node_t* node);
void deps_tree_free(dep_node_t* tree);

// Dependency serialization.

char* dep_node_serialize(dep_node_t* node);
int dep_node_deserialize(dep_node_t* root, char* serialized);
