// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <util.h>

#include <math.h>

void opts_init(opts_t* opts) {
	opts->opts = NULL;
	opts->count = 0;
}

void opts_free(opts_t* opts) {
	for (size_t i = 0; i < opts->count; i++) {
		char* const opt = opts->opts[i];

		if (!opt)
			continue;

		free(opt);
	}

	if (opts->opts)
		free(opts->opts);
}

void opts_add(opts_t* opts, char const* opt) {
	// only realloc if new count is larger than nearest POT of count
	// XXX I wonder if this'll be compiled down to an lzcnt or bsr instruction!

	if (opts->count) {
		size_t const pot = 1 << (size_t) log2((double) opts->count);

		if (opts->count + 1 > pot)
			opts->opts = realloc(opts->opts, 2 * pot * sizeof *opts->opts);
	}

	else
		opts->opts = malloc(sizeof *opts->opts);

	// then, just set the option

	opts->opts[opts->count++] = strdup(opt);
}
