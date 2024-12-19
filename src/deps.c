// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <common.h>

#include <deps.h>

#include <cmd.h>
#include <logging.h>

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

int deps_build(dep_node_t* tree) {
	return 0;
}
