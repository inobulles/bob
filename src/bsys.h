// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdbool.h>

typedef struct {
	char const* name;

	bool (*identify)(void);
	int (*build)(void);
} bsys_t;

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
