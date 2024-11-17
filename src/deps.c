// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <deps.h>

#include <logging.h>

int deps_download(flamingo_val_t* deps) {
	// Download (git) or symlink (local) all the dependencies to the dependencies directory.

	for (size_t i = 0; i < deps->vec.count; i++) {
		flamingo_val_t* const val = deps->vec.elems[i];

		// Find instance values.

		flamingo_val_t* kind = NULL;

		flamingo_val_t* local_path = NULL;

		flamingo_val_t* git_url = NULL;
		flamingo_val_t* git_branch = NULL;

		for (size_t j = 0; j < val->inst.scope->vars_size; j++) {
			flamingo_val_t* const inner = val->inst.scope->vars[j].val;

			if (flamingo_cstrcmp(inner->name, "kind", inner->name_size) == 0) {
				kind = inner;
			}

			else if (flamingo_cstrcmp(inner->name, "local_path", inner->name_size) == 0) {
				local_path = inner;
			}

			else if (flamingo_cstrcmp(inner->name, "git_url", inner->name_size) == 0) {
				git_url = inner;
			}

			else if (flamingo_cstrcmp(inner->name, "git_branch", inner->name_size) == 0) {
				git_branch = inner;
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

		if (flamingo_cstrcmp(kind->str.str, "local", kind->str.size) == 0) {
			if (local_path == NULL) {
				LOG_FATAL("Local dependency must have a 'local_path' attribute." PLZ_REPORT);
				return -1;
			}

			if (local_path->kind != FLAMINGO_VAL_KIND_STR) {
				LOG_FATAL("Local dependency 'local_path' attribute must be a string." PLZ_REPORT);
				return -1;
			}

			// Create symlink from dependency to deps directory.
		}

		else if (flamingo_cstrcmp(kind->str.str, "local", kind->str.size) == 0) {
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

			// TODO Download git repo to deps directory.
		}

		else {
			LOG_FATAL("Unknown value for dependency 'kind': '%.*s'." PLZ_REPORT, (int) kind->str.size, kind->str.str);
		}
	}

	// Just do a BFS here, going down the tree.
	// Probably I should have a 'get-deps' command, which does this step and returns a list of paths to these dependencies and probably also the edges of the dependency graph so we can get a fuller picture.
	// Does it make sense here to download all of them to a common deps directory for all Bob projects for reuse, Poetry-style?
	// I guess so, and then the name on the filesystem could be `'%s.local' % strhash(realpath(element))` for local stuff and `'%s.git' % (strhash(url)^ strhash(branch))` for stuff downloaded from git.
	// Once all the downloading is done, do the smart building thing where cores are allocated and reallocated to dependencies dynamically.
	// This is gonna be pretty complex so I should probably bring this out into a deps.c file.

	return 0;
}
