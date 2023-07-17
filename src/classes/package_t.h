// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

// XXX usually we'd want this to only be local to package.c's TU, but here we'd like to access the package object outside of it
//     so we make it accessible through this package_t.h header

typedef struct {
	char* entry;
	char* unique;
	char* name;
	char* description;
	char* version;
	char* author;
	char* organization;
	char* www;
} package_t;

