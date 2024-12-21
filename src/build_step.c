// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <build_step.h>

#include <assert.h>
#include <string.h>

size_t build_step_count = 0;
build_step_t* build_steps = NULL;

static int merge(void* data) {
	build_step_t* const last = &build_steps[build_step_count - 1];

	last->data = realloc(last->data, (last->data_count + 1) * sizeof *last->data);
	assert(last->data != NULL);

	last->data[last->data_count++] = data;
	return 0;
}

int add_build_step(uint64_t unique, char const* name, build_step_cb_t cb, void* data) {
	// Check if the last build step is the same as this one.
	// If it is, merge the two.

	if (build_step_count == 0) {
		goto no_last;
	}

	build_step_t* const last = &build_steps[build_step_count - 1];

	if (last->unique == unique) {
		assert(strcmp(last->name, name) == 0);
		assert(cb == last->cb);

		return merge(data);
	}

no_last:

	// Actually add new build step.

	build_steps = realloc(build_steps, ++build_step_count * sizeof *build_steps);
	assert(build_steps != NULL);
	build_step_t* const step = &build_steps[build_step_count - 1];

	step->unique = unique;
	step->name = name;
	step->cb = cb;
	step->data_count = 1;

	step->data = malloc(sizeof *build_steps->data);
	assert(step->data != NULL);

	step->data[0] = data;

	return 0;
}

void free_build_steps(void) {
	if (build_steps == NULL) {
		return;
	}

	for (size_t i = 0; i < build_step_count; i++) {
		free(build_steps[i].data);
	}

	free(build_steps);
	build_steps = NULL;
}

int run_build_steps(void) {
	for (size_t i = 0; i < build_step_count; i++) {
		build_step_t* const step = &build_steps[i];

		if (step->cb(step->data_count, step->data) < 0) {
			return -1;
		}
	}

	return 0;
}
