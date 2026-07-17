// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Aymeric Wibo

/*
 * Utility functions for dishing out external commands and reading their output.
 *
 *                       ______
 *                      /\     ".
 *                      \/__     |
 *               _.------:::|""- |
 *              =_=       )::.   `.
 *                "---og-':::|    |
 *        O       .-"""""""-:|    | LS
 *        |     .'    .---.  \    `.
 *        |     | ST j     i |_._._|
 *        |     |    t     ) '     `.
 *        |    /`.    >---i /:|     |
 *        |   /  |  .'   .';:::     |
 *        |.-' .'`.'  .-' |:::|    _|
 *        |,--'.-"_.-"    |:::|.-"" |
 *        " ,_=`-;-'     _>-'"      |
 *        ."|\.;--.  _.-'   _      .'
 * o=========| (__d-:"     (U)    _|
 *      __i|:' | .'/         _.-'" .
 *      \ |:| .' |'    _,.-'":     :
 *    -===|:' | .'_,-'"        : ::
 *       _\|_.' |"            :  :
 *      "-------'              :  :   LX-5 Command Cyborg
 *          \____\                       - Side View -
 */

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

/**
 * Initialize a command with an argument list.
 *
 * Arguments are passed as a variadic list, terminated by NULL.
 * Pass NULL as the first argument after 'cmd' for an empty argument list.
 *
 * @param cmd Command to initialize.
 * @param ... NULL-terminated list of argument strings (first should be the executable name).
 */
void cmd_create(cmd_t* cmd, ...);

/**
 * Append a single argument to the command.
 *
 * @param cmd Command to modify.
 * @param arg Argument string to append.
 */
void cmd_add(cmd_t* cmd, char const* arg);

/**
 * Append a printf-formatted argument to the command.
 *
 * @param cmd Command to modify.
 * @param fmt printf format string.
 * @param ... Format arguments.
 */
__attribute__((__format__(__printf__, 2, 3))) void cmd_addf(cmd_t* cmd, char const* fmt, ...);

/**
 * Append all elements of an argv array to the command.
 *
 * This skips all '--' arguments.
 * I'm not entirely sure why anymore and my comment sucks, but it was done in the following commit:
 *
 * c41cd09c sh: Don't require the setting of `POSIXLY_CORRECT` on Linux
 *
 * @param cmd Command to modify.
 * @param argc Number of arguments in 'argv'.
 * @param argv Argument array.
 */
void cmd_add_argv(cmd_t* cmd, int argc, char* argv[]);

typedef enum {
	CMD_REDIRECT = true,
	CMD_NO_REDIRECT = false,
} cmd_redirect_t;

typedef enum {
	CMD_FORCE_REDIRECT = true,
	CMD_NO_FORCE_REDIRECT = false,
} cmd_force_redirect_t;

/**
 * Configure whether the command's stdout and stderr are captured into 'cmd_t.out_buf'.
 *
 * In build debugging mode ('BOB_BUILD_DEBUGGING=1'), redirect is suppressed unless 'force' is also set.
 * Set 'force' when you really nneed the output of this command for something, and that something would break otherwise.
 *
 * @param cmd Command to configure.
 * @param redirect Whether to redirect output.
 * @param force Override build debugging mode suppression.
 */
void cmd_set_redirect(cmd_t* cmd, cmd_redirect_t redirect, cmd_force_redirect_t force);

/**
 * Queue data to be written to the command's stdin when it is spawned.
 *
 * @param cmd Command to configure.
 * @param data String to write to stdin (copied internally).
 */
void cmd_prepare_stdin(cmd_t* cmd, char* data);

/**
 * Check whether an executable exists and is in '$PATH'.
 *
 * @param cmd Executable name or path to look up.
 * @return True if the executable was found, false otherwise.
 */
bool cmd_exists(char const* cmd);

/**
 * Replace the current process with the command via 'execv'.
 *
 * @param cmd Command to execute.
 * @return -1 on failure, doesn't return on success.
 */
int cmd_exec_inplace(cmd_t* cmd);

/**
 * Spawn the command and wait on it.
 *
 * Collects its output into 'cmd_t.out_buf' if redirecting.
 * Sets 'cmd_t.rv' and 'cmd_t.sig'.
 *
 * @param cmd Command to execute.
 * @return 0 on success, -1 on failure, or non-zero exit.
 */
int cmd_exec(cmd_t* cmd);

/**
 * Return the captured output buffer.
 *
 * If the process was terminated by a signal, a message describing the signal is appended.
 * Only valid after {@link cmd_exec} with 'CMD_REDIRECT'.
 *
 * @param cmd Executed command.
 * @return Pointer to the output string (owned by 'cmd'; do not free).
 */
char* cmd_read_out(cmd_t* cmd);

/**
 * Log the result of a command execution, optionally writing output to a '.log' file next to 'cookie'.
 *
 * On failure, the output is always logged.
 * On success, only if 'log_success' is true and there is output.
 *
 * @param cmd Executed command.
 * @param cookie Cookie path used to derive the log file path (may be NULL to skip log file).
 * @param prefix Optional prefix shown before the status message (may be NULL).
 * @param infinitive Verb in infinitive form, used in failure message (e.g. "compile").
 * @param past Verb in past tense, used in success message (e.g. "compiled").
 * @param log_success Whether to print output on success.
 */
void cmd_log(cmd_t* cmd, char const* cookie, char const* prefix, char const* infinitive, char const* past, bool log_success);

/**
 * Print the command's argument list to stdout. Intended for debugging.
 *
 * @param cmd Command to print.
 */
void cmd_print(cmd_t* cmd);

/**
 * Free all resources owned by the command (argument list, output buffer, open file descriptors).
 *
 * Safe to call on a zero-initialized 'cmd_t' that was never executed.
 * You can annotate a 'cmd_t' with {@link CMD_CLEANUP} in order to run this automatically when it goes out of scope.
 *
 * @param cmd Command to free.
 */
void cmd_free(cmd_t* cmd);

#define CMD_CLEANUP __attribute__((cleanup(cmd_free)))
