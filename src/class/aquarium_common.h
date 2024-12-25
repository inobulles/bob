// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

typedef struct {
	char* template;
	char* kernel;

	// Set but not used by Aquarium.

	char* cookie;
} aquarium_state_t;

// TODO Put the preliminary aquarium check in here as a simple inline function.
