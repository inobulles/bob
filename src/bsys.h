// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdbool.h>

typedef struct bsys_t bsys_t;

struct bsys_t {
	char const* name;
	char const* key;

	bool (*identify)(void);
	int (*setup)(void);
	int (*build)(void);
	int (*install)(char const* prefix);
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
int bsys_build(bsys_t const* bsys);
int bsys_run(bsys_t const* bsys);
int bsys_install(bsys_t const* bsys);
