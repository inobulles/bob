#include <util.h>

#include <classes/rustc.h>

#include <errno.h>
#include <sys/stat.h>

typedef struct {
	char* path;
} rustc_t;

// foreign method binding

WrenForeignMethodFn rustc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", rustc_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", rustc_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "compile(_)", rustc_compile)

	// unknown

	return wren_unknown_foreign;
}

// helpers

static void rustc_init(rustc_t* rustc) {
	rustc->path = strdup("rustc");
}

// constructor/destructor

void rustc_new(WrenVM* vm) {
	CHECK_ARGC("CC.new", 0, 0)

	rustc_t* const rustc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *rustc);
	rustc_init(rustc);
}

void rustc_del(void* _rustc) {
	rustc_t* const rustc = _rustc;

	if (rustc->path)
		free(rustc->path);
}

// getters

void rustc_get_path(WrenVM* vm) {
	CHECK_ARGC("RustC.path", 0, 0)

	rustc_t* const rustc = foreign;
	wrenSetSlotString(vm, 0, rustc->path);
}

// setters

void rustc_set_path(WrenVM* vm) {
	CHECK_ARGC("CC.path=", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	rustc_t* const rustc = foreign;
	char const* const path = wrenGetSlotString(vm, 1);

	if (rustc->path)
		free(rustc->path);

	rustc->path = strdup(path);
}

// methods

void rustc_compile(WrenVM* vm) {
	CHECK_ARGC("RustC.compile", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	rustc_t* const rustc = foreign;
	char const* const _path = wrenGetSlotString(vm, 1);

	// declarations which must come before first goto

	char* out_path = NULL;

	// get absolute path or source file, hashing it, and getting output path

	char* const path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
		goto done;
	}

	uint64_t const hash = hash_str(path);
	if (asprintf(&out_path, "%s/%lx.o", bin_path, hash)) {}

	// if the output simply doesn't yet exist, don't bother doing anything, compile

	struct stat sb;

	if (stat(out_path, &sb) < 0) {
		if (errno == ENOENT)
			goto compile;

		LOG_ERROR("RustC.compile: stat(\"%s\"): %s", out_path, strerror(errno))
		goto done;
	}

	// if the source file is newer than the output, compile

	time_t out_mtime = sb.st_mtime;

	if (stat(path, &sb) < 0)
		LOG_ERROR("RustC.compile: stat(\"%s\"): %s", path, strerror(errno))

	if (sb.st_mtime >= out_mtime)
		goto compile;

	// don't need to compile!

	goto done;

	// actually compile

compile: {}

	// construct exec args
	// see: https://medium.com/@squanderingtime/manually-linking-rust-binaries-to-support-out-of-tree-llvm-passes-8776b1d037a4

	exec_args_t* const exec_args = exec_args_new(5, rustc->path, "--emit=obj", path, "-o", out_path);
	exec_args_save_out(exec_args, PIPE_STDERR); // both warning & errors go through stderr

	// if we've got colour support, force it in the compiler
	// we do this, because compiler output is piped
	// see: https://github.com/rust-lang/rustfmt/pull/2137

	if (colour_support)
		exec_args_add(exec_args, "--color=always");

	// finally, add task to compile asynchronously

	add_task(TASK_KIND_COMPILE, strdup(_path), exec_args);

	// clean up

done:

	if (path)
		free(path);
}
