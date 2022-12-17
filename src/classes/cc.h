#pragma once

#include <unistd.h>

#include <sys/stat.h>

#include "../util.h"

typedef struct {
	pid_t pid;
	char* name;

	int result;
	exec_args_t* exec_args;
} cc_proc_t;

typedef struct {
	bool debug;
	char* path;

	// TODO make this a hashset

	char** opts;
	size_t opts_len;

	cc_proc_t* cc_procs;
	size_t cc_procs_len;
} cc_t;

// helpers

static void cc_internal_add_opt(cc_t* cc, char const* opt) {
	cc->opts = realloc(cc->opts, ++cc->opts_len * sizeof *cc->opts);
	cc->opts[cc->opts_len - 1] = strdup(opt);
}

static int cc_internal_add_lib(cc_t* cc, char const* lib) {
	exec_args_t* exec_args = exec_args_new(4, "pkg-config", "--libs", "--cflags", lib);
	exec_args_save_out(exec_args, PIPE_STDOUT);

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		goto err;
	}

	char* const orig_opts = exec_args_read_out(exec_args, PIPE_STDOUT);
	char* opts = orig_opts;

	char* opt;

	while ((opt = strsep(&opts, " "))) {
		if (*opt == '\n') {
			continue;
		}

		cc_internal_add_opt(cc, opt);
	}

	free(orig_opts);

err:

	exec_args_del(exec_args);
	return rv;
}

static void cc_init(cc_t* cc) {
	cc->debug = true; // TODO be able to choose between various build types when running the bob command, and 'CC.debug' should default to that obviously
	cc->path = strdup("cc");

	cc->opts = NULL;
	cc->opts_len = 0;

	cc->cc_procs = NULL;
	cc->cc_procs_len = 0;

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
	FILE* fp = NULL;

	char* orig_headers = NULL;
	char* orig_prev = NULL;

	char* out_path = NULL;
	char* opts_path = NULL;

	// get absolute path or source file, hashing it, and getting output path

	char* const path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
		goto done;
	}

	uint64_t const hash = hash_str(path);

	if (asprintf(&out_path, "%s/%lx.o", bin_path, hash)) {}
	if (asprintf(&opts_path, "%s/%lx.opts", bin_path, hash)) {}

	// if the output simply doesn't yet exist, don't bother doing anything, compile

	struct stat sb;

	if (stat(out_path, &sb) < 0) {
		if (errno == ENOENT) {
			goto compile;
		}

		LOG_ERROR("CC.compile: stat(\"%s\"): %s", out_path, strerror(errno))
		goto done;
	}

	// if the source file is newer than the output, compile

	time_t out_mtime = sb.st_mtime;

	if (stat(path, &sb) < 0) {
		LOG_ERROR("CC.compile: stat(\"%s\"): %s", path, strerror(errno))
	}

	if (sb.st_mtime >= out_mtime) {
		goto compile;
	}

	// if one of the dependencies (i.e. included headers) of the source file is more recent than the output, compile
	// for this, parse the makefile rule output by the preprocessor
	// see: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html#Preprocessor-Options
	// also see: https://wiki.sei.cmu.edu/confluence/display/c/STR06-C.+Do+not+assume+that+strtok%28%29+leaves+the+parse+string+unchanged

	exec_args = exec_args_new(5, cc->path, "-MM", "-MT", "", path);
	exec_args_save_out(exec_args, PIPE_STDOUT | PIPE_STDERR);

	for (size_t i = 0; i < cc->opts_len; i++) {
		exec_args_add(exec_args, cc->opts[i]);
	}

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		goto done;
	}

	orig_headers = exec_args_read_out(exec_args, PIPE_STDOUT);

	if (!orig_headers) {
		goto done;
	}

	char* headers = orig_headers;
	char* header;

	while ((header = strsep(&headers, " "))) {
		if (*header == ':' || *header == '\\' || !*header) {
			continue;
		}

		size_t const len = strlen(header);

		if (header[len - 1] == '\n') {
			header[len - 1] = '\0';
		}

		// if header is more recent than the output, compile

		if (stat(header, &sb) < 0) {
			LOG_WARN("CC.compile: stat(\"%s\"): %s", header, strerror(errno));
			continue;
		}

		if (sb.st_mtime >= out_mtime) {
			goto compile;
		}
	}

	// if one of the options changed, compile
	// if options file doesn't exist, compile

	fp = fopen(opts_path, "r");

	if (!fp) {
		goto compile;
	}

	orig_prev = file_read_str(fp, file_get_size(fp));
	char* prev = orig_prev;

	char* prev_opt;

	for (size_t i = 0;; i++) {
		prev_opt = strsep(&prev, "\n");

		bool const prev_done = !prev_opt || !*prev_opt || *prev_opt == '\n';
		bool const opts_done = i == cc->opts_len;

		if (opts_done && prev_done) {
			break;
		}

		if (prev_done != opts_done) {
			fclose(fp);
			goto compile;
		}

		char* const opt = cc->opts[i];
		prev_opt[strlen(prev_opt)] = '\0'; // remove newline

		if (strcmp(opt, prev_opt)) {
			fclose(fp);
			goto compile;
		}
	}

	// don't need to compile!

	goto done;

	// actually compile

compile: {}

	// construct exec args

	if (exec_args) {
		exec_args_del(exec_args);
	}

	exec_args = exec_args_new(5, cc->path, "-c", path, "-o", out_path);
	exec_args_save_out(exec_args, PIPE_STDERR); // both warning & errors go through stderr

	// if we've got colour support, force it in the compiler
	// we do this, because compiler output is piped
	// '-fcolor-diagnostics' also works, but only on clang

	if (colour_support) {
		exec_args_add(exec_args, "-fdiagnostics-color=always");
	}

	fp = fopen(opts_path, "w");

	if (cc->debug) { // TODO I don't like how this is its separate thing... put it in cc->opts
		exec_args_add(exec_args, "-g");
	}

	for (size_t i = 0; i < cc->opts_len; i++) {
		exec_args_add(exec_args, cc->opts[i]);
		fprintf(fp, "%s\n", cc->opts[i]);
	}

	// finally, actually compile asynchronously

	pid_t const pid = execute_async(exec_args);

	// add pid to list of processes

	cc->cc_procs = realloc(cc->cc_procs, ++cc->cc_procs_len * sizeof *cc->cc_procs);
	cc_proc_t* cc_proc = &cc->cc_procs[cc->cc_procs_len - 1];

	cc_proc->pid = pid;
	cc_proc->name = strdup(_path);

	cc_proc->exec_args = exec_args;

	// clean up

done:

	if (fp) {
		fclose(fp);
	}

	if (orig_headers) {
		free(orig_headers);
	}

	if (orig_prev) {
		free(orig_prev);
	}

	if (out_path) {
		free(out_path);
	}

	if (opts_path) {
		free(opts_path);
	}

	if (path) {
		free(path);
	}
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
