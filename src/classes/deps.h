#pragma once

#include "../util.h"

// methods

static void deps_git(WrenVM* vm) {
	CHECK_ARGC("Deps.git", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	char const* const url = wrenGetSlotString(vm, 1);

	// hash url to know where to clone repository

	uint64_t const hash = hash_str(url);
	char* repo_path;

	if (!asprintf(&repo_path, "%s/%lx.git", bin_path, hash))
		;

	// check that the repository has not already been cloned, and clone it
	// e.g., a previous dependency could've already done that

	exec_args_t* args = NULL;
	int rv = EXIT_SUCCESS;

	struct stat sb;

	if (stat(repo_path, &sb) || !S_ISDIR(sb.st_mode)) {
		args = exec_args_new(6, "git", "clone", "--depth", "1", url, repo_path);

		rv = execute(args);
		exec_args_del(args);

		if (rv) {
			LOG_ERROR("Failed to clone remote git repository '%s'", url)
			goto err;
		}
	}

	// execute bob in the cloned repository, and pass our bin path to it

	args = exec_args_new(6, init_name, "-C", repo_path, "-o", bin_path, "test");

	rv = execute(args);
	exec_args_del(args);

	if (rv) {
		LOG_ERROR("Failed to build git repository '%s'", url)
	}

err:

	wrenSetSlotDouble(vm, 0, rv);
}

// foreign method binding

static WrenForeignMethodFn deps_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "git(_)", deps_git)

	// unknown

	return unknown_foreign;
}
