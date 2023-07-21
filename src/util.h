// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <common.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <wren.h>

#include <logging.h>

// global variables/functions

extern char const* rel_bin_path;
extern char* bin_path;
extern char const* init_name;
extern char const* curr_instr;
extern char const* prefix;
extern char const* project_path;
extern bool gen_lsp_config;

void usage(void);

// useful macros

#define BIND_FOREIGN_METHOD(_static_, _signature, fn) \
	if (static_ == (_static_) && !strcmp(signature, (_signature))) \
		return (fn);

// this may be a contender for ugliest macro I've ever written
// not sure though, I've written some pretty ugly macros in my time
// yeah actually screw that, the '__UI_LOG' macro in 'aquabsd.alps.ui/private.h' is like 10 times worse

#define CHECK_ARGC(fn_name, argc_little, argc_big) \
	char const* const __fn_name = (fn_name); /* accessible by future macros */ \
	int argc = wrenGetSlotCount(vm) - 1; \
	\
	/* be pessimistic and set return value to null right off the bat */ \
	/* foreign values in slot 0 really come in to put a thorn in our side, but that's alright, we too cool for that to stop us 😎 */ \
	/* getting the value of the foreign slot doesn't work when calling 'wrenSetSlotNewForeign' though, because we can't just read what's in 'classSlot', so we'll have to make an exception to '.new' methods */ \
	\
	__attribute__((unused)) void* foreign = NULL; \
	\
	if (strcmp(strrchr(__fn_name, '.'), ".new")) { \
		if (wrenGetSlotType(vm, 0) == WREN_TYPE_FOREIGN) \
			foreign = wrenGetSlotForeign(vm, 0); \
		\
		wrenSetSlotNull(vm, 0); \
	} \
	\
	if (argc < (argc_little) || argc > (argc_big)) { \
		if ((argc_little) == (argc_big)) \
			LOG_WARN("'%s' not passed right number of arguments (got %d, expected between %d & %d)", __fn_name, argc, (argc_little), (argc_big)) \
		\
		else \
			LOG_WARN("'%s' not passed right number of arguments (got %d, expected %d)", __fn_name, argc, (argc_little)) \
		\
		return; \
	}

#define ASSERT_ARG_TYPE(i, type) \
	if (wrenGetSlotType(vm, (i)) != (type)) { \
		LOG_WARN("'%s' argument " #i " is of incorrect type (expected '" #type "')", __fn_name) \
		return; \
	}

#define CLEANUP_STR __attribute__((cleanup(strfree)))

// wren stuff

typedef struct {
	WrenConfiguration config;
	WrenVM* vm;

	// configuration file

	FILE* fp;
	char* src;
} state_t;

void wren_shush(bool shut_up);
void wren_unknown_foreign(WrenVM* vm);

WrenForeignMethodFn wren_bind_foreign_method(WrenVM* vm, char const* module, char const* class, bool static_, char const* sig);
WrenForeignClassMethods wren_bind_foreign_class(WrenVM* vm, char const* module, char const* class);

int wren_setup_vm(state_t* state);
void wren_clean_vm(state_t* state);

int wren_call(state_t* state, char const* class, char const* sig, char const** str_ref);
int wren_call_args(state_t* state, char const* class, char const* sig, int argc, char** argv);

int wren_read_map(state_t* state, char const* name, WrenHandle** map_handle_ref, size_t* keys_len_ref);

// pipe stuff

typedef enum {
	PIPE_STDOUT = 0b01,
	PIPE_STDERR = 0b10,
} pipe_kind_t;

typedef struct {
	pipe_kind_t kind;

	int in;
	int out;

	int err_in;
	int err_out;
} pipe_t;

int pipe_create(pipe_t* self);
void pipe_child(pipe_t* self);
void pipe_parent(pipe_t* self);
char* pipe_read_out(pipe_t* self, pipe_kind_t kind);
void pipe_free(pipe_t* self);

// options stuff

typedef struct {
	// TODO make this a hashset

	char** opts;
	size_t count;
} opts_t;

void opts_init(opts_t* opts);
void opts_free(opts_t* opts);

void opts_add(opts_t* opts, char const* opt);

// exec args stack object stuff

typedef struct {
	size_t len; // includes NULL sentinel
	char** args;

	pipe_t pipe;
} exec_args_t;

exec_args_t* exec_args_new(size_t len, ...);
void exec_args_save_out(exec_args_t* self, pipe_kind_t kind);
char* exec_args_read_out(exec_args_t* self, pipe_kind_t kind);
void exec_args_add(exec_args_t* self, char const* arg);
void exec_args_add_opts(exec_args_t* self, opts_t* opts);

__attribute__((__format__(__printf__, 2, 3)))
void exec_args_fmt(exec_args_t* self, char const* fmt, ...);

__attribute__((unused))
void exec_args_print(exec_args_t* self);

void exec_args_del(exec_args_t* self);

// process manipulation stuff

int wait_for_process(pid_t pid);
pid_t execute_async(exec_args_t* self);
int execute(exec_args_t* _exec_args);

// task manipulation stuff

typedef enum {
	TASK_KIND_GENERIC,
	TASK_KIND_COMPILE,
} task_kind_t;

typedef enum {
	TASK_HOOK_KIND_POST,
} task_hook_kind_t;

typedef struct task_t task_t; // forward declaration
typedef int (*task_hook_t) (task_t* task, void* data);

struct task_t {
	bool completed;
	task_kind_t kind;

	pid_t pid;
	char* name;

	int result;
	exec_args_t* exec_args;

	// hooks

	task_hook_t post_hook;
	void* post_hook_data;
};

task_t* add_task(task_kind_t kind, char* name, exec_args_t* exec_args);
void task_hook(task_t* self, task_hook_kind_t kind, task_hook_t hook, void* data);
size_t wait_for_tasks(task_kind_t kind);

// filesystem stuff

size_t file_get_size(FILE* fp);
char* file_read_str(FILE* fp, size_t size);
int path_write_str(char* path, char* str);
time_t be_frugal(char* src_path, char* out_path);
int mkdir_recursive(char const* _path);
char* copy_recursive(char const* _src, char const* dest);
char* remove_recursive(char const* path);

// misc stuff

void strfree(char* const* str_ref);
uint64_t hash_str(char const* str);
int navigate_project_path(void);
int ensure_out_path(void);
int fix_out_path_owner(void);
char const* install_prefix(void);
