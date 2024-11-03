// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdlib.h>

extern size_t max_jobs;

int set_max_jobs(size_t ncpu);
size_t ncpu(void);
