// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <logging.h>
#include <ncpu.h>

#include <unistd.h>

size_t max_jobs = 0;

size_t ncpu(void) {
	if (max_jobs > 0) {
		return max_jobs;
	}

	size_t const n = sysconf(_SC_NPROCESSORS_ONLN);

	if (n < 0) {
		LOG_WARN("Couldn't get number of CPU's, defaulting to 1");
		return 1;
	}

	return n;
}