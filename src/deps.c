// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <deps.h>

#include <cmd.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int build(char const* human, char const* path) {
	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, init_name, "-o", abs_out_path, "-p", install_prefix, "-C", path, "build", NULL);

	LOG_INFO("%s" CLEAR ": Building dependency...", human);
	int const rv = cmd_exec(&cmd);
	cmd_log(&cmd, NULL, human, "build dependency", "built dependency", false);

	return rv;
}

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

		char* CLEANUP_STR dep_path = NULL;
		char* CLEANUP_STR human = NULL;

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

			char* const CLEANUP_STR path = strndup(local_path->str.str, local_path->str.size);
			assert(path != NULL);

			char* const CLEANUP_STR abs_path = realpath(path, NULL);

			if (abs_path == NULL) {
				assert(errno != ENOMEM);
				LOG_FATAL("realpath(\"%s\"): %s", path, strerror(errno));
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

			if (symlink(path, dep_path) < 0 && errno != EEXIST) {
				LOG_FATAL("link(\"%s\", \"%s\"): %s", path, dep_path, strerror(errno));
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

			char* const CLEANUP_STR tmp = strndup(git_url->str.str, git_url->str.size);
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
		// Now, build it.

downloaded:

		assert(dep_path != NULL);
		assert(human != NULL);

		if (build(human, dep_path) < 0) {
			return -1;
		}
	}

	return 0;
}
