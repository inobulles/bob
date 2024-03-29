// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#include <util.h>
#include <classes/cc.h>

#include <errno.h>

#include <sys/stat.h>

typedef struct {
	char* path;
	opts_t opts;

	FILE* lsp_config;
} cc_t;

// foreign method binding

WrenForeignMethodFn cc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", cc_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", cc_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "add_lib(_)", cc_add_lib)
	BIND_FOREIGN_METHOD(false, "add_opt(_)", cc_add_opt)
	BIND_FOREIGN_METHOD(false, "compile(_)", cc_compile)

	// unknown

	return wren_unknown_foreign;
}

// helpers

static int internal_add_lib(cc_t* cc, char const* lib) {
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

		opts_add(&cc->opts, opt);
	}

	free(orig_opts);

err:

	exec_args_del(exec_args);
	return rv;
}

// constructor/destructor

#define LSP_CONFIG "compile_commands.json"

void cc_new(WrenVM* vm) {
	CHECK_ARGC("CC.new", 0, 0)

	cc_t* const cc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *cc);

	cc->path = strdup("cc");
	opts_init(&cc->opts);

	// this is very annoying and dumb so whatever just disable it for everyone

	opts_add(&cc->opts, "-Wno-unused-command-line-argument");

	if (!gen_lsp_config) {
		return;
	}

	cc->lsp_config = fopen(LSP_CONFIG, "w");

	if (!cc->lsp_config) {
		LOG_ERROR("Failed to create CC LSP configuration file");
		return;
	}

	validate_gitignore(LSP_CONFIG);
	validate_gitignore(".cache");

	fprintf(cc->lsp_config, "[\n");
}

void cc_del(void* _cc) {
	cc_t* const cc = _cc;

	strfree(&cc->path);
	opts_free(&cc->opts);

	if (cc->lsp_config) {
		fprintf(cc->lsp_config, "]\n");
		fclose(cc->lsp_config);
	}
}

// getters

void cc_get_path(WrenVM* vm) {
	CHECK_ARGC("CC.path", 0, 0)

	cc_t* const cc = foreign;
	wrenSetSlotString(vm, 0, cc->path);
}

// setters

void cc_set_path(WrenVM* vm) {
	CHECK_ARGC("CC.path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	strfree(&cc->path);
	cc->path = strdup(path);
}

// methods

void cc_add_lib(WrenVM* vm) {
	CHECK_ARGC("CC.add_lib", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const lib = wrenGetSlotString(vm, 1);

	int rv = internal_add_lib(cc, lib);

	if (rv) {
		LOG_WARN("'CC.add_lib' failed to add '%s' (error code is %d)", lib, rv);
	}
}

void cc_add_opt(WrenVM* vm) {
	CHECK_ARGC("CC.add_opt", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const opt = wrenGetSlotString(vm, 1);

	opts_add(&cc->opts, opt);
}

void cc_compile(WrenVM* vm) {
	CHECK_ARGC("CC.compile", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	cc_t* const cc = foreign;
	char const* const _path = wrenGetSlotString(vm, 1);

	// declarations which must come before first goto

	exec_args_t* exec_args = NULL;
	FILE* fp = NULL;

	char* CLEANUP_STR orig_headers = NULL;
	char* CLEANUP_STR orig_prev = NULL;

	char* CLEANUP_STR out_path = NULL;
	char* CLEANUP_STR opts_path = NULL;

	// get absolute path or source file, hashing it, and getting output path

	char* const CLEANUP_STR path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
		goto done;
	}

	uint64_t const hash = hash_str(path);

	if (asprintf(&out_path, "%s/%lx.o", bin_path, hash)) {}
	if (asprintf(&opts_path, "%s/%lx.opts", bin_path, hash)) {}

	// if we're generating LSP config, always compile

	if (cc->lsp_config) {
		goto compile;
	}

	// check if source has been modified since it was last compiled

	time_t const out_mtime = be_frugal(path, out_path);

	if (out_mtime < 0) {
		goto done;
	}

	if (!out_mtime) {
		goto compile;
	}

	// if one of the dependencies (i.e. included headers) of the source file is more recent than the output, compile
	// for this, parse the makefile rule output by the preprocessor
	// see: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html#Preprocessor-Options
	// also see: https://wiki.sei.cmu.edu/confluence/display/c/STR06-C.+Do+not+assume+that+strtok%28%29+leaves+the+parse+string+unchanged

	exec_args = exec_args_new(5, cc->path, "-MM", "-MT", "", path);
	exec_args_save_out(exec_args, PIPE_STDOUT | PIPE_STDERR);
	exec_args_add_opts(exec_args, &cc->opts);
	exec_args_fmt(exec_args, "-I%s", bin_path); // TODO share more code between this and the actual compilation command

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		char* const CLEANUP_STR header_errors = exec_args_read_out(exec_args, PIPE_STDERR);
		LOG_WARN("CC.compile(\"%s\"): Failed to look for header dependencies%s", path, header_errors ? ":" : "")

		if (header_errors) {
			fprintf(stderr, "%s", header_errors);
		}

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

		struct stat sb;

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
	// TODO this really ought to be part of opts.c, so that others can rely on this
	//      but maybe I'd like an even more complete system for dependencies?
	//      because different classes have different dependencies, idk, something to think about

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
		bool const opts_done = i == cc->opts.count;

		if (opts_done && prev_done) {
			break;
		}

		if (prev_done != opts_done) {
			fclose(fp);
			goto compile;
		}

		char* const opt = cc->opts.opts[i];
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

	if (!fp) {
		LOG_WARN("fopen(\"%s\"): %s", opts_path, strerror(errno))
		goto no_opts;
	}

	exec_args_add_opts(exec_args, &cc->opts);

	for (size_t i = 0; fp && i < cc->opts.count; i++) {
		fprintf(fp, "%s\n", cc->opts.opts[i]);
	}

	// add the output directory as an include search path
	// we do this at the end so as not to override any custom include search paths

	exec_args_fmt(exec_args, "-I%s", bin_path);

	// if we're writing an LSP config, add the exec_args to it

	if (cc->lsp_config) {
		fprintf(cc->lsp_config, "\t{\n\t\t\"arguments\": [\n");

		for (size_t i = 0; i < exec_args->len - 1; i++) {
			char const* const comma = i < exec_args->len - 2 ? "," : "";
			fprintf(cc->lsp_config, "\t\t\t\"%s\"%s\n", exec_args->args[i], comma);
		}

		fprintf(cc->lsp_config, "\t\t],\n");
		fprintf(cc->lsp_config, "\t\t\"file\": \"%s\",\n", path);
		fprintf(cc->lsp_config, "\t\t\"directory\": \"%s\"\n", project_path);
		fprintf(cc->lsp_config, "\t},\n");
	}

	// finally, add task to compile asynchronously

no_opts:

	add_task(TASK_KIND_COMPILE, strdup(_path), exec_args);

	// clean up

done:

	if (fp) {
		fclose(fp);
	}
}
