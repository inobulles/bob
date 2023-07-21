// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <common.h>

#include <stdio.h>

#include <util.h>
#include <wren.h>

// macros

#define INSTALL_MAP "install"
#define PACKAGE_MAP "packages"

// common stuff between instructions

int setup_env(char* working_dir);
int install(state_t* state);
int package(state_t* state, char const* format_str, char* name, char* _out);

// actual instructions

int do_build();
int do_run(int argc, char** argv);
int do_install(void);
int do_skeleton(int argc, char** argv);
int do_test(void);
int do_package(int argc, char** argv);
