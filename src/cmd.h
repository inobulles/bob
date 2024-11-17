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

	// Pipe.

	int in;
	int out;
} cmd_t;

void cmd_create(cmd_t* cmd, ...);
void cmd_add(cmd_t* cmd, char const* arg);
__attribute__((__format__(__printf__, 2, 3))) void cmd_addf(cmd_t* cmd, char const* fmt, ...);
void cmd_add_argv(cmd_t* cmd, int argc, char* argv[]);
int cmd_exec_inplace(cmd_t* cmd);
pid_t cmd_exec_async(cmd_t* cmd);
int cmd_exec(cmd_t* cmd);
char* cmd_read_out(cmd_t* cmd);
void cmd_log(cmd_t* cmd, char const* cookie, char const* prefix, char const* infinitive, char const* past, bool log_success);
void cmd_print(cmd_t* cmd);
void cmd_free(cmd_t* cmd);

#define CMD_CLEANUP __attribute__((cleanup(cmd_free)))
