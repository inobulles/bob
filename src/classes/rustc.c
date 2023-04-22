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

typedef struct {
	rustc_t* rustc;
	uint64_t hash;
	char* cargo_dir_path;
} compile_post_hook_data_t;

static int compile_post_hook(task_t* task, void* _data) {
	int rv = -1;

	(void) task;

	compile_post_hook_data_t* const data = _data;

	char* __attribute__((cleanup(strfree))) src = NULL;
	if (asprintf(&src, "%s/target/debug/lib_%lx.so", data->cargo_dir_path, data->hash)) {}

	char* __attribute__((cleanup(strfree))) dest = NULL;
	if (asprintf(&dest, "%s/%lx.o", bin_path, data->hash)) {}

	if (copy_recursive(src, dest) < 0) {
		LOG_ERROR("Failed to copy %s to %s", src, dest)
		goto err;
	}

	// success

	rv = 0;

err:

	free(data->cargo_dir_path);
	free(data);

	return rv;
}

// constructor/destructor

void rustc_new(WrenVM* vm) {
	CHECK_ARGC("RustC.new", 0, 0)

	rustc_t* const rustc = wrenSetSlotNewForeign(vm, 0, 0, sizeof *rustc);

	rustc->path = strdup("cargo");
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
	CHECK_ARGC("RustC.path=", 1, 1)

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

	char* const __attribute__((cleanup(strfree))) path = realpath(_path, NULL);

	if (!path) {
		LOG_WARN("'%s' does not exist", path)
		return;
	}

	uint64_t const hash = hash_str(path);
	if (asprintf(&out_path, "%s/%lx.o", bin_path, hash)) {}

	// if the output simply doesn't yet exist, don't bother doing anything, compile

	struct stat sb;

	if (stat(out_path, &sb) < 0) {
		if (errno == ENOENT)
			goto compile;

		LOG_ERROR("RustC.compile: stat(\"%s\"): %s", out_path, strerror(errno))
		return;
	}

	// if the source file is newer than the output, compile

	time_t out_mtime = sb.st_mtime;

	if (stat(path, &sb) < 0)
		LOG_ERROR("RustC.compile: stat(\"%s\"): %s", path, strerror(errno))

	if (sb.st_mtime >= out_mtime)
		goto compile;

	// don't need to compile!

	return;

	// actually compile

compile: {}

	// create Cargo.toml file
	// TODO in the future, it'd be nice if we could pass a Package to the RustC constructor to fill in the [package] section of the manifest
	// TODO what's the risk for injection here?

	char* __attribute__((cleanup(strfree))) cargo_dir_path = NULL;
	if (asprintf(&cargo_dir_path, "%s/%lx_Cargo.toml.d", bin_path, hash)) {}

	if (mkdir(cargo_dir_path, 0770) < 0 && errno != EEXIST) {
		LOG_ERROR("RustC.compile: mkdir(\"%s\"): %s", cargo_dir_path, strerror(errno))
		return;
	}

	char* __attribute__((cleanup(strfree))) cargo_path = NULL;
	if (asprintf(&cargo_path, "%s/Cargo.toml", cargo_dir_path)) {}

	FILE* const fp = fopen(cargo_path, "w");

	if (!fp) {
		LOG_ERROR("RustC.compile: fopen(\"%s\"): %s", cargo_path, strerror(errno))
		return;
	}

	fprintf(fp, "[package]\n");
	fprintf(fp, "name = '_%lx'\n", hash);
	fprintf(fp, "version = '0.0.0'\n");

	fprintf(fp, "[lib]\n");
	fprintf(fp, "crate-type = ['dylib']\n");
	fprintf(fp, "name = '_%lx'\n", hash);
	fprintf(fp, "path = '%s'\n", path);

	fclose(fp);

	// construct exec args

	exec_args_t* const exec_args = exec_args_new(4, rustc->path, "build", "--manifest-path", cargo_path);
	exec_args_save_out(exec_args, PIPE_STDERR); // both warning & errors go through stderr

	// if we've got colour support, force it in the compiler
	// we do this, because compiler output is piped
	// see: https://github.com/rust-lang/rustfmt/pull/2137

	if (colour_support)
		exec_args_add(exec_args, "--color=always");

	// add task to compile asynchronously

	task_t* const task = add_task(TASK_KIND_COMPILE, strdup(_path), exec_args);

	// add post hook

	compile_post_hook_data_t* const data = calloc(1, sizeof *data);

	data->rustc = rustc;
	data->hash = hash;
	data->cargo_dir_path = strdup(cargo_dir_path);

	task_hook(task, TASK_HOOK_KIND_POST, compile_post_hook, data);
}
