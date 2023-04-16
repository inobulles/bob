#pragma once

#include <common.h>

#include <stdio.h>

#include <util.h>
#include <wren.h>

// macros

#define INSTALL_MAP "install"

// common stuff between instructions

void setup_env(char* working_dir);
int read_map(state_t* state, char const* name, WrenHandle** map_handle_ref, size_t* keys_len_ref);

// actual instructions

int do_build(void);
int do_run(int argc, char** argv);
int do_install(void);
int do_skeleton(int argc, char** argv);
int do_test(void);
int do_package(int argc, char** argv);
