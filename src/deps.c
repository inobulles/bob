// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <deps.h>

#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// REMME (But don't forget to copy the comment about -o; I think this is important!)

__attribute__((unused)) static int build(char const* human, char const* path) {
	cmd_t CMD_CLEANUP cmd = {0};
	// TODO Wait no the -o passed to a dependency when building should really be its own output path. -p is the only thing that should be in common. That being said, that raises the issue of if a dependency wants to use a different output path. Maybe there should be a function in Bob to set the output path to something else instead of having an -o switch?
	cmd_create(&cmd, init_name, "-p", install_prefix, "-C", path, "build", NULL);

	LOG_INFO("%s" CLEAR ": Building dependency...", human);
	int const rv = cmd_exec(&cmd);
	cmd_log(&cmd, NULL, human, "build dependency", "built dependency", false);

	return rv;
}

typedef struct {
	char* path;
	char* human;
} dep_t;

static int deps_download(flamingo_val_t* deps_vec, dep_t* deps, uint64_t* hash) {
	assert(*hash == 0);

	// Download (git) or symlink (local) all the dependencies to the dependencies directory.

	for (size_t i = 0; i < deps_vec->vec.count; i++) {
		flamingo_val_t* const val = deps_vec->vec.elems[i];

		// Find instance values.

		flamingo_val_t* kind = NULL;

		flamingo_val_t* local_path = NULL;

		flamingo_val_t* git_url = NULL;
		flamingo_val_t* git_branch = NULL;

		for (size_t j = 0; j < val->inst.scope->vars_size; j++) {
			flamingo_var_t* const inner = &val->inst.scope->vars[j];

			if (flamingo_cstrcmp(inner->key, "kind", inner->key_size) == 0) {
				kind = inner->val;
			}

			else if (flamingo_cstrcmp(inner->key, "local_path", inner->key_size) == 0) {
				local_path = inner->val;
			}

			else if (flamingo_cstrcmp(inner->key, "git_url", inner->key_size) == 0) {
				git_url = inner->val;
			}

			else if (flamingo_cstrcmp(inner->key, "git_branch", inner->key_size) == 0) {
				git_branch = inner->val;
			}
		}

		// Make sure everything checks out and take action.

		if (kind == NULL) {
			LOG_FATAL("Dependency must have a 'kind' attribute." PLZ_REPORT);
			return -1;
		}

		if (kind->kind != FLAMINGO_VAL_KIND_STR) {
			LOG_FATAL("Dependency 'kind' attribute must be a string." PLZ_REPORT);
			return -1;
		}

		char* STR_CLEANUP dep_path = NULL;
		char* STR_CLEANUP human = NULL;

		if (flamingo_cstrcmp(kind->str.str, "local", kind->str.size) == 0) {
			if (local_path == NULL) {
				LOG_FATAL("Local dependency must have a 'local_path' attribute." PLZ_REPORT);
				return -1;
			}

			if (local_path->kind != FLAMINGO_VAL_KIND_STR) {
				LOG_FATAL("Local dependency 'local_path' attribute must be a string." PLZ_REPORT);
				return -1;
			}

			// Generate path for dependency in deps directory.

			char* const STR_CLEANUP path = strndup(local_path->str.str, local_path->str.size);
			assert(path != NULL);

			char* const STR_CLEANUP abs_path = realerpath(path);

			if (abs_path == NULL) {
				LOG_FATAL("Could not get local dependency at '%s'.", path);
				return -1;
			}

			human = strrchr(abs_path, '/');
			assert(human++ != NULL);
			human = strdup(human);
			assert(human != NULL);

			uint64_t const hash = str_hash(abs_path, strlen(abs_path));

			asprintf(&dep_path, "%s/%s.%" PRIx64 ".local", deps_path, human, hash);
			assert(dep_path != NULL);

			// Create symlink from dependency to deps directory.
			// We're creating a symlink and not a hard one because, since we're depending the unique name of the dependency on it's original path, we want to break thing if it's ever moved.

			if (symlink(abs_path, dep_path) < 0 && errno != EEXIST) {
				LOG_FATAL("link(\"%s\", \"%s\"): %s", abs_path, dep_path, strerror(errno));
				return -1;
			}
		}

		else if (flamingo_cstrcmp(kind->str.str, "git", kind->str.size) == 0) {
			if (git_url == NULL) {
				LOG_FATAL("Git dependency must have a 'git_url' attribute." PLZ_REPORT);
				return -1;
			}

			if (git_branch == NULL) {
				LOG_FATAL("Git dependency must have a 'git_branch' attribute." PLZ_REPORT);
				return -1;
			}

			if (git_url->kind != FLAMINGO_VAL_KIND_STR) {
				LOG_FATAL("Git dependency 'git_url' attribute must be a string." PLZ_REPORT);
				return -1;
			}

			if (git_branch->kind != FLAMINGO_VAL_KIND_STR) {
				LOG_FATAL("Git dependency 'git_branch' attribute must be a string." PLZ_REPORT);
				return -1;
			}

			// Get dependency path of git repo.

			char* const STR_CLEANUP tmp = strndup(git_url->str.str, git_url->str.size);
			assert(tmp != NULL);

			human = strrchr(tmp, '/');
			human = strdup(human == NULL ? tmp : human + 1);
			assert(human != NULL);

			uint64_t const hash =
				str_hash(git_url->str.str, git_url->str.size) ^
				str_hash(git_branch->str.str, git_branch->str.size);

			asprintf(&dep_path, "%s/%s.%" PRIx64 ".git", deps_path, human, hash);
			assert(dep_path != NULL);

			if (access(dep_path, F_OK) == 0) {
				goto downloaded;
			}

			// If nothing exists there yet, clone the repo.

			cmd_t CMD_CLEANUP cmd = {0};

			cmd_create(&cmd, "git", "clone", NULL);
			cmd_addf(&cmd, "%.*s", (int) git_url->str.size, git_url->str.str);
			cmd_addf(&cmd, "%s/%s.%" PRIx64 ".git", deps_path, human, hash);
			cmd_add(&cmd, "--depth");
			cmd_add(&cmd, "1");
			cmd_add(&cmd, "--branch");
			cmd_addf(&cmd, "%.*s", (int) git_branch->str.size, git_branch->str.str);

			LOG_INFO("%s" CLEAR ": Git cloning...", human);

			bool const failure = cmd_exec(&cmd) < 0;
			cmd_log(&cmd, NULL, human, "git clone", "git cloned", false);

			if (failure) {
				return -1;
			}
		}

		else {
			LOG_FATAL("Unknown value for dependency 'kind': '%.*s'." PLZ_REPORT, (int) kind->str.size, kind->str.str);
			return -1;
		}

		// If we're here, we've successfully downloaded the dependency.

downloaded:

		assert(dep_path != NULL);
		assert(human != NULL);

		// Create the dependency object.

		*hash ^= str_hash(dep_path, strlen(dep_path));

		deps[i].path = strdup(dep_path);
		assert(deps[i].path != NULL);

		deps[i].human = strdup(human);
		assert(deps[i].path != NULL);
	}

	return 0;
}

dep_node_t* deps_tree(flamingo_val_t* deps_vec) {
	// Start off by going though all our direct dependencies and making sure they're downloaded.
	// TODO Free the dependency list.

	dep_t deps[deps_vec->vec.count + 1]; // Because a count of 0 is UB.
	uint64_t hash = 0;

	if (deps_download(deps_vec, deps, &hash) < 0) {
		return NULL;
	}

	// Create the root node of the dependency tree.
	// TODO Free the tree when there are errors.

	dep_node_t* const tree = calloc(1, sizeof *tree);
	assert(tree != NULL);

	tree->is_root = true;
	tree->path = NULL;

	tree->child_count = 0;
	tree->children = NULL;

	// Then, attempt to read the cached dependency tree.
	// If the hashes don't match, the dependency list changed, so we'll have to rebuild the tree.

	char* STR_CLEANUP serialized = NULL;

	char* STR_CLEANUP hash_path;
	asprintf(&hash_path, "%s/deps.hash", out_path);
	assert(hash_path != NULL);

	char* STR_CLEANUP tree_path;
	asprintf(&tree_path, "%s/deps.tree", out_path);
	assert(tree_path != NULL);

	FILE* const hash_f = fopen(hash_path, "r");

	if (hash_f == NULL) {
		LOG_INFO("No cached dependency tree found, building it.");
		goto build_tree;
	}

	uint64_t read_hash;
	fscanf(hash_f, "%" PRIx64, &read_hash);
	fclose(hash_f);

	if (read_hash != hash) {
		LOG_INFO("Dependency vector changed, rebuilding dependency tree.");
		goto build_tree;
	}

	FILE* const tree_f = fopen(tree_path, "r");

	if (tree_f == NULL) {
		LOG_WARN("Could not open dependency tree file '%s', rebuilding it.", tree_path);
		goto build_tree;
	}

	fseek(tree_f, 0, SEEK_END);
	size_t const tree_size = ftell(tree_f);
	rewind(tree_f);

	serialized = calloc(1, tree_size + 1);
	assert(serialized != NULL);
	fread(serialized, 1, tree_size, tree_f);

	fclose(tree_f);

	if (dep_node_deserialize(tree, serialized) < 0) {
		return NULL;
	}

	return tree;

	// Build the tree.

build_tree:;

	for (size_t i = 0; i < deps_vec->vec.count; i++) {
		dep_t* const dep = &deps[i];

		// Run the 'dep-tree' command on the dependency and add the resulting dependency trees to ours.

		cmd_t CMD_CLEANUP cmd;
		cmd_create(&cmd, init_name, "-p", install_prefix, "-C", dep->path, "dep-tree", NULL);

		int const rv = cmd_exec(&cmd);
		char* const STR_CLEANUP out = cmd_read_out(&cmd);

		if (rv < 0) {
			LOG_FATAL("Failed to get dependency tree of '%s'%s", dep->human, out ? ":" : ".");
			printf("%s", out);
			return NULL;
		}

		dep_node_t node;

		if (dep_node_deserialize(&node, out) < 0) {
			LOG_FATAL("Failed to deserialize dependency tree of '%s'.", dep->human);
			return NULL;
		}

		node.is_root = false;
		node.path = strdup(dep->path);

		tree->children = realloc(tree->children, (tree->child_count + 1) * sizeof *tree->children);
		assert(tree->children != NULL);
		tree->children[tree->child_count++] = node;
	}

	// Write out tree hash.

	FILE* f = fopen(hash_path, "w");

	if (f == NULL) {
		LOG_FATAL("Could not open dependency hash file '%s' for writing: %s", hash_path, strerror(errno));
		return NULL;
	}

	fprintf(f, "%" PRIx64, hash);
	fclose(f);

	// Write out tree.

	f = fopen(tree_path, "w");

	if (f == NULL) {
		LOG_FATAL("Could not open dependency tree file '%s' for writing: %s", tree_path, strerror(errno));
		return NULL;
	}

	if (serialized != NULL) {
		free(serialized);
	}

	serialized = dep_node_serialize(tree);
	fprintf(f, "%s", serialized);
	fclose(f);

	return tree;
}
