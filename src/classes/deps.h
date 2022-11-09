#pragma once

#include "../util.h"

// methods

static void deps_git(WrenVM* vm) {
	CHECK_ARGC("Deps.git", 1, 1)
	char const* url = wrenGetSlotString(vm, 1);

	// hash url to know where to clone repository

	uint64_t const hash = hash_str(url);

	// clone remote repository

	exec_args_t* args = exec_args_new(4, "/usr/local/bin/git", "clone", "--depth", "1");
	exec_args_add(args, (char*) url);
	exec_args_fmt(args, "%s/%lx.git", bin_path, hash);

	int rv = execute(args);
	exec_args_del(args);

	if (rv) {
		LOG_ERROR("Failed to clone remote git repository '%s'", url)
		return;
	}

	// TODO execute bob in the cloned repository, and pass our bin path to it
	//      in the future, it'll know exactly what to do in a variety of different cases
}

// foreign method binding

static WrenForeignMethodFn deps_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "git(_)", deps_git)

	// unknown

	return unknown_foreign;
}
