#pragma once

#include <common.h>

#include <stdio.h>

#include <util.h>
#include <wren.h>

// stuff in common between instructions

void setup_env(char* working_dir);
int read_installation_map(state_t* state, WrenHandle** map_handle_ref, size_t* keys_len_ref);

// actual instructions

int do_build(void);
int do_run(int argc, char** argv);
int do_install(void);
int do_skeleton(int argc, char** argv);
int do_test(void);
