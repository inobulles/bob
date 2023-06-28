#include <util.h>

#include <classes/deps.h>

#include <sys/stat.h>

// foreign method binding

WrenForeignMethodFn deps_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "git(_)", deps_git)
	BIND_FOREIGN_METHOD(true, "git(_,_)", deps_git)

	// unknown

	return wren_unknown_foreign;
}

// methods

void deps_git(WrenVM* vm) {
	CHECK_ARGC("Deps.git", 1, 2)
	bool const has_branch = argc == 2;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	if (has_branch) {
		ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)
	}

	char const* const url = wrenGetSlotString(vm, 1);
	char const* const branch = has_branch ? wrenGetSlotString(vm, 2) : NULL;

	// hash url to know where to clone repository

	uint64_t const hash = hash_str(url);

	char* repo_path;
	if (!asprintf(&repo_path, "%s/%lx.git", bin_path, hash)) {}

	// check that the repository has not already been cloned, and clone it
	// e.g., a previous dependency could've already done that

	exec_args_t* args = NULL;
	int rv = EXIT_SUCCESS;

	struct stat sb;

	if (stat(repo_path, &sb) || !S_ISDIR(sb.st_mode)) {
		args = exec_args_new(6, "git", "clone", url, repo_path, "--depth", "1");
		exec_args_save_out(args, PIPE_STDERR);

		if (branch) {
			exec_args_add(args, "--branch");
			exec_args_add(args, branch);
		}

		rv = execute(args);
		char* const CLEANUP_STR err = exec_args_read_out(args, PIPE_STDERR);

		exec_args_del(args);

		if (rv) {
			if (err != NULL) {
				err[strlen(err) - 1] = '\0';
				LOG_ERROR("Failed to clone remote git repository '%s':\n%s", url, err)
			}

			else {
				LOG_ERROR("Failed to clone remote git repository '%s'", url)
			}

			goto err;
		}
	}

	// return the path to the repository

	wrenSetSlotString(vm, 0, repo_path);

err:

	free(repo_path);
}
