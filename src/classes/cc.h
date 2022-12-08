#pragma once

#include "../util.h"

#include <sys/unistd.h>
#include <unistd.h>

typedef struct {
	bool debug;
	char* path;

	// TODO make this a hashset

	char** opts;
	size_t opts_len;

	pid_t* compilation_processes;
	size_t compilation_processes_len;
} cc_t;

// helpers

static void cc_internal_add_opt(cc_t* cc, char const* opt) {
	cc->opts = realloc(cc->opts, ++cc->opts_len * sizeof *cc->opts);
	cc->opts[cc->opts_len - 1] = strdup(opt);
}

static int cc_internal_add_lib(cc_t* cc, char const* lib) {
	exec_args_t* exec_args = exec_args_new(4, "pkg-config", "--libs", "--cflags", lib);
	exec_args_save_out(exec_args, true);

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	char* opts = exec_args_read_out(exec_args);

	char* opt;

	while ((opt = strsep(&opts, " "))) {
		if (*opt == '\n') {
			continue;
		}

		cc_internal_add_opt(cc, opt);
	}

	free(opts);

err:

	exec_args_del(exec_args);
	return rv;
}

static void cc_init(cc_t* cc) {
	cc->debug = true; // TODO be able to choose between various build types when running the bob command, and 'CC.debug' should default to that obviously
	cc->path = strdup("cc");

	cc->opts = NULL;
	cc->opts_len = 0;

	cc->compilation_processes = NULL;
	cc->compilation_processes_len = 0;

	// this is very annoying and dumb so whatever just disable it for everyone

	cc_internal_add_opt(cc, "-Wno-unused-command-line-argument");
}

// constructor/destructor

static void cc_new(WrenVM* vm) {
	CHECK_ARGC("CC.new", 0, 0)

	cc_t* const cc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *cc);
	cc_init(cc);
}

static void cc_del(void* _cc) {
	cc_t* const cc = _cc;

	if (cc->path) {
		free(cc->path);
	}

	for (size_t i = 0; i < cc->opts_len; i++) {
		char* const opt = cc->opts[i];

		if (!opt) {
			continue;
		}

		free(opt);
	}

	if (cc->opts) {
		free(cc->opts);
	}
}

// getters

static void cc_get_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug", 0, 0)

	cc_t* const cc = foreign;
	wrenSetSlotBool(vm, 0, cc->debug);
}

static void cc_get_path(WrenVM* vm) {
	CHECK_ARGC("CC.path", 0, 0)

	cc_t* const cc = foreign;
	wrenSetSlotString(vm, 0, cc->path);
}

// setters

static void cc_set_debug(WrenVM* vm) {
	CHECK_ARGC("CC.debug=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_BOOL)

	cc_t* const cc = foreign;
	bool const debug = wrenGetSlotBool(vm, 1);

	cc->debug = debug;
}

static void cc_set_path(WrenVM* vm) {
	CHECK_ARGC("CC.path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (cc->path) {
		free(cc->path);
	}

	cc->path = strdup(path);
}

// methods

static void cc_add_lib(WrenVM* vm) {
	CHECK_ARGC("CC.add_lib", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const lib = wrenGetSlotString(vm, 1);

	int rv = cc_internal_add_lib(cc, lib);

	if (rv) {
		LOG_WARN("'CC.add_lib' failed to add '%s' (error code is %d)", lib, rv);
	}
}

static void cc_add_opt(WrenVM* vm) {
	CHECK_ARGC("CC.add_opt", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const opt = wrenGetSlotString(vm, 1);

	cc_internal_add_opt(cc, opt);
}

static void cc_compile(WrenVM* vm) {
	CHECK_ARGC("CC.compile", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const _path = wrenGetSlotString(vm, 1);

	// declarations which must come before first goto

	exec_args_t* exec_args = NULL;

	// get absolute path or source file, hashing it, and getting output path

	char* const path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
		return;
	}

	uint64_t const hash = hash_str(path);
	char* out_path;

	if (asprintf(&out_path, "%s/%lx.o", bin_path, hash))
		;

	// if the output simply doesn't yet exist, don't bother doing anything, compile

	struct stat sb;

	if (stat(out_path, &sb) < 0) {
		if (errno == ENOENT) {
			goto compile;
		}

		LOG_ERROR("CC.compile: stat(\"%s\"): %s", out_path, strerror(errno))
		goto stat_err;
	}

	// if the source file is newer than the output, compile

	size_t out_mtime = sb.st_mtime;

	if (stat(path, &sb) < 0) {
		LOG_ERROR("CC.compile: stat(\"%s\"): %s", path, strerror(errno))
	}

	if (sb.st_mtime >= out_mtime) {
		goto compile;
	}

	// if one of the dependencies (i.e. included headers) of the source file is more recent than the output, compile
	// for this, parse the makefile rule output by the preprocessor
	// see: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html#Preprocessor-Options

	exec_args = exec_args_new(5, cc->path, "-MM", "-MT", "", path);
	exec_args_save_out(exec_args, true);

	for (size_t i = 0; i < cc->opts_len; i++) {
		exec_args_add(exec_args, cc->opts[i]);
	}

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		goto cpp_err; // CPP as in C PreProcessor
	}

	char* headers = exec_args_read_out(exec_args);

	exec_args_del(exec_args);
	exec_args = NULL;

	if (!headers) {
		goto cpp_err;
	}

	char* header;

	while ((header = strsep(&headers, " "))) {
		if (*header == ':' || *header == '\\' || !*header) {
			continue;
		}

		size_t len = strlen(header);

		if (header[len - 1] == '\n') {
			header[--len] = 0;
		}

		// if header is more recent than the output, compile

		if (stat(header, &sb) < 0) {
			LOG_WARN("CC.compile: stat(\"%s\"): %s", header, strerror(errno));
			continue;
		}

		if (sb.st_mtime >= out_mtime) {
			free(headers);
			goto compile;
		}
	}

	free(headers);

	// TODO what happens if compile options change in the meantime?
	//      maybe I could hash the options list (XOR each option's hash together) and store that in 'bin/'?

	// don't need to compile!

	return;

	// actually compile

compile: {}

	LOG_FATAL("Compiling %s", path);

	// construct exec args

	exec_args = exec_args_new(5, cc->path, "-c", path, "-o", out_path);

	if (cc->debug) {
		exec_args_add(exec_args, "-g");
	}

	for (size_t i = 0; i < cc->opts_len; i++) {
		exec_args_add(exec_args, cc->opts[i]);
	}

	// finally, actually compile asynchronously

	pid_t const pid = execute_async(exec_args);

	// add pid to list of processes

	cc->compilation_processes = realloc(cc->compilation_processes, ++cc->compilation_processes_len * sizeof *cc->compilation_processes);
	cc->compilation_processes[cc->compilation_processes_len - 1] = pid;

	// clean up

cpp_err:

	exec_args_del(exec_args);

stat_err:

	free(out_path);
	free(path);
}

// foreign method binding

static WrenForeignMethodFn cc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "debug()", cc_get_debug)
	BIND_FOREIGN_METHOD(false, "path()", cc_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "debug=(_)", cc_set_debug)
	BIND_FOREIGN_METHOD(false, "path=(_)", cc_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "add_lib(_)", cc_add_lib)
	BIND_FOREIGN_METHOD(false, "add_opt(_)", cc_add_opt)
	BIND_FOREIGN_METHOD(false, "compile(_)", cc_compile)

	// unknown

	return unknown_foreign;
}
