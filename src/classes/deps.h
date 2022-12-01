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

	// check that the repository has not already been cloned
	// e.g., a previous dependency could've already done that

	struct stat sb;

	if (!stat(repo_path, &sb) && S_ISDIR(sb.st_mode)) {
		return;
	}

	// clone remote repository

	// exec_args_t* args = exec_args_new(6, "/usr/local/bin/git", "clone", "--depth", "1", url, repo_path);
	exec_args_t* args = exec_args_new(8, "/usr/local/bin/git", "clone", "--branch", "bob", "--depth", "1", url, repo_path);

	int rv = execute(args);
	exec_args_del(args);

	if (rv) {
		LOG_ERROR("Failed to clone remote git repository '%s'", url)
		return;
	}

	// execute bob in the cloned repository, and pass our bin path to it

	args = exec_args_new(6, init_name, "-C", repo_path, "-o", bin_path, "build");

	rv = execute(args);
	exec_args_del(args);

	if (rv) {
		LOG_ERROR("Failed to build git repository '%s'", url)
	}
}

// foreign method binding

static WrenForeignMethodFn deps_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "git(_)", deps_git)

	// unknown

	return unknown_foreign;
}
