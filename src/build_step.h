// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef int (*build_step_cb_t)(size_t data_count, void** data);

typedef struct {
	uint64_t unique;
	char const* name;
	build_step_cb_t cb;

	size_t data_count;
	void** data;
} build_step_t;

int add_build_step(uint64_t unique, char const* name, build_step_cb_t cb, void* data);
void free_build_steps(void);

int run_build_steps(void);
