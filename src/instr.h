#pragma once

#include <stdio.h>

#include <wren.h>

#include <util.h>
#include <logging.h>

// Wren-related functions
// TODO should this even be here??

typedef struct {
	WrenConfiguration config;
	WrenVM* vm;

	// configuration file

	FILE* fp;
	char* src;
} state_t;

WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* sig);
WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class);

int wren_setup_vm(state_t* state);
void wren_clean_vm(state_t* state);

int wren_call(state_t* state, char const* class, char const* sig, char const** str_ref);
int wren_call_args(state_t* state, char const* class, char const* sig, int argc, char** argv);

// stuff in common between instructions

void setup_env(char* working_dir);
int read_installation_map(state_t* state, WrenHandle** map_handle_ref, size_t* keys_len_ref);

// actual instructions

int do_build(void);
int do_run(int argc, char** argv);
int do_install(void);
int do_skeleton(int argc, char** argv);
int do_test(void);
