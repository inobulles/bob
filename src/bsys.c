// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <bsys.h>
#include <cmd.h>
#include <deps.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

bsys_t const* bsys_identify(void) {
	for (size_t i = 0; i < sizeof(BSYS) / sizeof(BSYS[0]); i++) {
		if (BSYS[i]->identify()) {
			return BSYS[i];
		}
	}

	return NULL;
}

int bsys_dep_tree(bsys_t const* bsys, int argc, char* argv[]) {
	dep_node_t* tree = NULL;
	int rv = -1;

	if (bsys->dep_tree == NULL) {
		rv = 0;
		goto no_tree;
	}

	// Parse the arguments as a list of hashes.

	uint64_t* const hashes = malloc(argc * sizeof *hashes);
	assert(hashes != NULL);

	for (int i = 0; i < argc; i++) {
		hashes[i] = strtoull(argv[i], NULL, 16);
	}

	// Create dependency tree.

	bool circular;
	tree = bsys->dep_tree(argc, hashes, &circular);
	free(hashes);

	if (tree == NULL && circular) {
		printf(BOB_DEPS_CIRCULAR);
		return 0;
	}

	if (tree != NULL) {
		rv = 0;
	}

	assert(!circular);

	// Serialize and output it.

no_tree:;

	char* const STR_CLEANUP serialized = tree == NULL ? strdup("") : dep_node_serialize(tree);
	printf(DEP_TAG_START "%s" DEP_TAG_END, serialized);

	deps_tree_free(tree);
	return rv;
}

static int do_build_deps(bsys_t const* bsys) {
	if (bsys->dep_tree == NULL || bsys->build_deps == NULL) {
		return 0;
	}

	// Create dependency tree.

	bool circular;
	dep_node_t* const tree = bsys->dep_tree(0, NULL, &circular);

	if (tree == NULL) {
		if (circular) {
			LOG_FATAL("Dependency tree is circular.");
		}

		return -1;
	}

	assert(!circular);

	// Build each dependency.

	int const rv = bsys->build_deps(tree);
	deps_tree_free(tree);

	return rv;
}

int bsys_build(bsys_t const* bsys) {
	if (bsys->build == NULL) {
		LOG_WARN("%s build system does not have a build step; nothing to build!", bsys->name);
		return 0;
	}

	// Building installs to a temporary prefix.
	// It would be nice to only do this when -p is passed, but unfortunately that's not possible because some build steps can depend on preinstalled artifacts.

	if (install_prefix == NULL) {
		install_prefix = default_tmp_install_prefix;
	}

	// Build dependencies here.

	if (build_deps && do_build_deps(bsys) < 0) {
		return -1;
	}

	// Ensure the output path exists.
	// TODO Do this with mkdir_recursive? Do this in main.c?

	if (bsys->key != NULL) {
		if (mkdir_wrapped(bsys_out_path, 0755) < 0 && errno != EEXIST) {
			LOG_FATAL("mkdir(\"%s\"): %s", bsys_out_path, strerror(errno));
			return -1;
		}

		set_owner(bsys_out_path);
	}

	// Actually build.

	return bsys->build();
}

static void prepend_env(char const* name, char const* fmt, ...) {
	va_list args;

	va_start(args, fmt);

	char* STR_CLEANUP val;
	vasprintf(&val, fmt, args);
	assert(val != NULL);

	va_end(args);

	char* const prev = getenv(name);
	int rv = -1;

	if (prev == NULL) {
		rv = setenv(name, val, 1);
	}

	else {
		// We prepend because earlier entries searched first.

		char* STR_CLEANUP new;
		asprintf(&new, "%s:%s", val, prev);
		assert(new != NULL);

		rv = setenv(name, new, 1);
	}

	if (rv < 0) {
		LOG_WARN("setenv(\"%s\", \"%s\"): %s", name, val, strerror(errno));
	}
}

static int install(bsys_t const* bsys, bool default_to_tmp_prefix) {
	if (bsys->install == NULL) {
		LOG_WARN("%s: build system does not have an install step; nothing to install!", bsys->name);
		return 0;
	}

	// If defaulting to temporary prefix, set that if install prefix is not already set.
	// Otherwise, default to the final prefix (obviously).

	if (install_prefix == NULL) {
		if (default_to_tmp_prefix) {
			install_prefix = default_tmp_install_prefix;
		}

		else {
			install_prefix = default_final_install_prefix;
		}
	}

	// Run build step.

	if (bsys_build(bsys) < 0) {
		return -1;
	}

	// Actually install.

	return bsys->install();
}

static void setup_environment(void) {
	prepend_env("PATH", "%s/bin", install_prefix);

	prepend_env(
#if defined(__APPLE__)
		"DY" // dyld(1) doesn't recognize `LD_LIBRARY_PATH`.
#endif
		"LD_LIBRARY_PATH",
		"%s/lib",
		install_prefix
	);
}

int bsys_run(bsys_t const* bsys, int argc, char* argv[]) {
	if (install(bsys, true) < 0) {
		return -1;
	}

	if (bsys->run == NULL) {
		LOG_WARN("%s: build system does not have a run step; nothing to run!", bsys->name);
		return 0;
	}

	setup_environment();

	return bsys->run(argc, argv);
}

int bsys_sh(bsys_t const* bsys, int argc, char* argv[]) {
	if (install(bsys, true) < 0) {
		return -1;
	}

	setup_environment();

	// Actually run command.

	cmd_t __attribute__((cleanup(cmd_free))) cmd = {0};
	cmd_create(&cmd, NULL);

	if (argc == 0) {
		char const* const shell = getenv("SHELL");

		if (shell == NULL) {
			LOG_FATAL("No command passed to 'bob sh' and $SHELL not set.");
			return -1;
		}

		LOG_SUCCESS("Entering shell ('%s').", shell);
		cmd_add(&cmd, shell);
	}

	else {
		cmd_add_argv(&cmd, argc, argv);
	}

	if (cmd_exec_inplace(&cmd) < 0) {
		LOG_ERROR("Failed to run command.");
		return -1;
	}

	assert(false); // This should never be reached if 'cmd_exec_inplace' succeeds.
}

int bsys_install(bsys_t const* bsys) {
	return install(bsys, false);
}

int bsys_clean(bsys_t const* bsys) {
	if (bsys->clean == NULL) {
		return 0;
	}

	return bsys->clean();
}
