// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
	size_t len; // includes NULL sentinel
	char** args;
	int sig;
	int rv;
	bool redirect;

	// Pipe (stdout & stderr).

	char* out_buf;

	int in;
	int out;

	// Pipe (stdin).

	char* pending_stdin;

	int stdin_in;
	int stdin_out;
} cmd_t;

void cmd_create(cmd_t* cmd, ...);
void cmd_add(cmd_t* cmd, char const* arg);
__attribute__((__format__(__printf__, 2, 3))) void cmd_addf(cmd_t* cmd, char const* fmt, ...);
void cmd_add_argv(cmd_t* cmd, int argc, char* argv[]);

typedef enum {
	CMD_REDIRECT = true,
	CMD_NO_REDIRECT = false,
} cmd_redirect_t;

typedef enum {
	CMD_FORCE_REDIRECT = true,
	CMD_NO_FORCE_REDIRECT = false,
} cmd_force_redirect_t;

void cmd_set_redirect(cmd_t* cmd, cmd_redirect_t redirect, cmd_force_redirect_t force);
void cmd_prepare_stdin(cmd_t* cmd, char* data);

bool cmd_exists(char const* cmd);

int cmd_exec_inplace(cmd_t* cmd);
int cmd_exec(cmd_t* cmd);

char* cmd_read_out(cmd_t* cmd);
void cmd_log(cmd_t* cmd, char const* cookie, char const* prefix, char const* infinitive, char const* past, bool log_success);

void cmd_print(cmd_t* cmd);
void cmd_free(cmd_t* cmd);

#define CMD_CLEANUP __attribute__((cleanup(cmd_free)))
