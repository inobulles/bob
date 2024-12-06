// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

typedef struct dep_node_t dep_node_t;

struct dep_node_t {
	bool is_root; // The root dependency node is self.
	char* path;   // If is root, `path` will be `NULL`.

	size_t child_count;
	struct dep_node_t* children;
};

int deps_download(flamingo_val_t* deps);

// Dependency serialization.

char* dep_node_serialize(dep_node_t* node);
int dep_node_deserialize(dep_node_t* root, char* serialized);
