// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <deps.h>

#include <stdbool.h>

typedef struct bsys_t bsys_t;

struct bsys_t {
	char const* name;
	char const* key;

	bool (*identify)(void);
	int (*setup)(void);
	dep_node_t* (*dep_tree)(size_t path_len, uint64_t* path_hashes, bool* circular);
	int (*build_deps)(dep_node_t* tree, bool preinstall);
	int (*build)(char const* preinstall_prefix);
	int (*install)(char const* prefix);
	int (*run)(int argc, char* argv[]);
	void (*destroy)(void);
};

extern bsys_t const BSYS_BOB;
extern bsys_t const BSYS_MAKE_FREEBSD_PORT;
extern bsys_t const BSYS_MESON;
extern bsys_t const BSYS_CMAKE;
extern bsys_t const BSYS_CONFIGURE;
extern bsys_t const BSYS_GMAKE;
extern bsys_t const BSYS_CARGO;
extern bsys_t const BSYS_GO;

static bsys_t const* const BSYS[] = {
	&BSYS_BOB,
	&BSYS_MAKE_FREEBSD_PORT,
	&BSYS_MESON,
	&BSYS_CMAKE,
	&BSYS_CONFIGURE,
	&BSYS_GMAKE,
	&BSYS_CARGO,
	&BSYS_GO,
};

bsys_t const* bsys_identify(void);
int bsys_dep_tree(bsys_t const* bsys, int argc, char* argv[]);
int bsys_build(bsys_t const* bsys, char const* preinstall_prefix, bool build_deps);
int bsys_run(bsys_t const* bsys, int argc, char* argv[]);
int bsys_sh(bsys_t const* bsys, int argc, char* argv[]);
int bsys_install(bsys_t const* bsys);
