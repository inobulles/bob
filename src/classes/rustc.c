#include <util.h>

#include <classes/rustc.h>

#include <errno.h>
#include <sys/stat.h>

typedef struct {
	char* name;
	char* git;

	size_t feature_count;
	char** features;
} dep_t;

typedef struct {
	char* path;

	size_t dep_count;
	dep_t* deps;
} rustc_t;

// foreign method binding

WrenForeignMethodFn rustc_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "path()", rustc_get_path)

	// setters

	BIND_FOREIGN_METHOD(false, "path=(_)", rustc_set_path)

	// methods

	BIND_FOREIGN_METHOD(false, "add_dep(_,_)", rustc_add_dep)
	BIND_FOREIGN_METHOD(false, "add_dep(_,_,_)", rustc_add_dep)
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

	rustc->deps = NULL;
	rustc->dep_count = 0;
}

void rustc_del(void* _rustc) {
	rustc_t* const rustc = _rustc;

	if (rustc->path)
		free(rustc->path);

	// free dependencies

	for (size_t i = 0; i < rustc->dep_count; i++) {
		dep_t* const dep = &rustc->deps[i];

		free(dep->name);
		free(dep->git);

		for (size_t i = 0; i < dep->feature_count; i++)
			free(dep->features[i]);

		if (dep->features)
			free(dep->features);
	}

	if (rustc->deps)
		free(rustc->deps);
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

void rustc_add_dep(WrenVM* vm) {
	CHECK_ARGC("RustC.add_dep", 2, 3)
	bool const has_feature_list = argc == 3;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)
	ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)

	if (has_feature_list)
		ASSERT_ARG_TYPE(3, WREN_TYPE_LIST)

	rustc_t* const rustc = foreign;
	char const* const name = wrenGetSlotString(vm, 1);
	char const* const git = wrenGetSlotString(vm, 2);
	size_t const feature_list_len = has_feature_list ? wrenGetListCount(vm, 3) : 0;

	// extra argument checks

	if (has_feature_list && !feature_list_len)
		LOG_WARN("'%s' passed an empty list of features", __fn_name)

	// add dependency

	rustc->deps = realloc(rustc->deps, ++rustc->dep_count * sizeof *rustc->deps);
	dep_t* const dep = &rustc->deps[rustc->dep_count - 1];

	dep->name = strdup(name);
	dep->git = strdup(git);

	// read dependency features (if there are any)

	dep->feature_count = feature_list_len;
	dep->features = NULL;

	if (dep->feature_count)
		dep->features = malloc(dep->feature_count * sizeof *dep->features);

	wrenEnsureSlots(vm, 5); // we just need a single extra slot for each list element

	for (size_t i = 0; i < dep->feature_count; i++) {
		wrenGetListElement(vm, 3, i, 4);

		if (wrenGetSlotType(vm, 4) != WREN_TYPE_STRING) {
			LOG_WARN("'RustC.add_dep' list element %zu of argument 3 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const feature = wrenGetSlotString(vm, 4);
		dep->features[i] = strdup(feature);
	}
}

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

	fprintf(fp, "[dependencies]\n");

	for (size_t i = 0; i < rustc->dep_count; i++) {
		dep_t* const dep = &rustc->deps[i];

		fprintf(fp, "%s = { git = '%s', features = [", dep->name, dep->git);

		for (size_t i = 0; i < dep->feature_count; i++)
			fprintf(fp, "'%s', ", dep->features[i]);

		fprintf(fp, "] }\n");
	}

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
