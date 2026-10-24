// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

// TODO:
// - Check FreeBSD version for my patch to Linuxulator and warn user if FreeBSD version too old.
// - Run 'android update'?
// - './android sdk list --all' shows undownloaded stuff too.

#include <common.h>

#include <alloc.h>
#include <class/class.h>
#include <cmd.h>
#include <fsutil.h>
#include <logging.h>
#include <str.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__FreeBSD__)
# include <sys/module.h>
# include <sys/param.h>
#endif

#define ANDROID "Android"

static flamingo_val_t* require_val = NULL;
static flamingo_val_t* get_ndk_val = NULL;

static int get_dl_url(char** out_url) {
	char const* os;

#if defined(__APPLE__) && defined(__x86_64__)
	os = "darwin_x86_64";
#elif defined(__APPLE__) && defined(__aarch64__)
	os = "darwin_arm64";
#elif defined(__linux__) && defined(__x86_64__)
	os = "linux_x86_64";
#elif defined(__FreeBSD__) && defined(__x86_64__)
	if (modfind("linux64elf") < 0) {
		LOG_FATAL("FreeBSD linux64 kernel module not loaded (try 'kldload linux64')");
		return -1;
	}

	struct stat proc_sb;

	if (stat("/proc/self", &proc_sb) < 0) {
		LOG_FATAL("linprocfs not mounted on /proc (try 'mount -t linprocfs linprocfs /proc')");
		return -1;
	}

	os = "linux_x86_64";
#else
	LOG_FATAL("Unsupported platform for Android SDK");
	return -1;
#endif

	asprintf_c(out_url, "https://dl.google.com/android/cli/latest/%s/android", os);
	return 0;
}

static int ensure_downloaded(char* path) {
	// Check if already downloaded.

	if (access(path, X_OK) == 0) {
		return 0;
	}

	// Get download URL.

	char* STR_CLEANUP url = NULL;

	if (get_dl_url(&url) < 0) {
		return -1;
	}

	// Download with curl/fetch.

	LOG_INFO(ANDROID ": Downloading Android CLI tool...");

	cmd_t CMD_CLEANUP cmd = {0};

#if defined(__FreeBSD__)
	cmd_create(&cmd, "fetch", "-o", path, url, NULL);
#else
	cmd_create(&cmd, "curl", "-fsSL", "-o", path, url, NULL);
#endif

	if (cmd_exec(&cmd) < 0) {
		cmd_log(&cmd, NULL, ANDROID, "download", "downloaded", true);
		return -1;
	}

	cmd_log(&cmd, NULL, ANDROID, "download", "downloaded", true);

	// Make executable.

	if (chmod(path, 0755) < 0) {
		LOG_FATAL(ANDROID ": chmod(\"%s\"): %s", path, strerror(errno));
		return -1;
	}

	return 0;
}

static int get_tool_path(char** out_path) {
	char* STR_CLEANUP dir = NULL;
	asprintf_c(&dir, "%s/android", cache_path);

	char* path = NULL;
	asprintf_c(&path, "%s/android", dir);

	if (mkdir_recursive(dir, 0755) < 0) {
		LOG_FATAL(ANDROID ": mkdir_recursive(\"%s\"): %s", dir, strerror(errno));
		free(path);
		return -1;
	}

	if (ensure_downloaded(path) < 0) {
		free(path);
		return -1;
	}

	*out_path = path;
	return 0;
}

static int get_sdk_path(char** out_path) {
	char* path = NULL;
	asprintf_c(&path, "%s/android/sdk", cache_path);

	if (mkdir_recursive(path, 0755) < 0) {
		LOG_FATAL(ANDROID ": mkdir_recursive(\"%s\"): %s", path, strerror(errno));
		free(path);
		return -1;
	}

	*out_path = path;
	return 0;
}

static int require(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	if (args->count != 1) {
		LOG_FATAL(ANDROID ".require: Expected 1 argument, got %zu.", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(ANDROID ".require: Expected argument to be a string.");
		return -1;
	}

	flamingo_val_t* const component_val = args->args[0];
	char* const STR_CLEANUP component = strndup_c(component_val->str.str, component_val->str.size);

	// Get tool and SDK paths.

	char* STR_CLEANUP tool_path = NULL;

	if (get_tool_path(&tool_path) < 0) {
		return -1;
	}

	char* STR_CLEANUP sdk_path = NULL;

	if (get_sdk_path(&sdk_path) < 0) {
		return -1;
	}

	// Run: android sdk install --sdk=<sdk_path> <component>

	LOG_INFO(ANDROID ": Ensuring '%s' is installed...", component);

	char* STR_CLEANUP sdk_flag = NULL;
	asprintf_c(&sdk_flag, "--sdk=%s", sdk_path);

	cmd_t CMD_CLEANUP cmd = {0};
	cmd_create(&cmd, tool_path, "sdk", "install", sdk_flag, component, NULL);

	if (cmd_exec(&cmd) < 0) {
		cmd_log(&cmd, NULL, ANDROID, "install", "installed", true);
		return -1;
	}

	cmd_log(&cmd, NULL, ANDROID, "install", "installed", false);

	*rv = flamingo_val_make_none();
	return 0;
}

static int get_ndk(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	if (args->count != 3) {
		LOG_FATAL(ANDROID "._get_ndk: Expected 3 arguments, got %zu.", args->count);
		return -1;
	}

	if (args->args[0]->kind != FLAMINGO_VAL_KIND_INST) {
		LOG_FATAL(ANDROID "._get_ndk: Expected first argument to be a Toolchain instance.");
		return -1;
	}

	if (args->args[1]->kind != FLAMINGO_VAL_KIND_STR) {
		LOG_FATAL(ANDROID "._get_ndk: Expected second argument to be a string.");
		return -1;
	}

	if (args->args[2]->kind != FLAMINGO_VAL_KIND_INT) {
		LOG_FATAL(ANDROID "._get_ndk: Expected third argument to be an integer.");
		return -1;
	}

	flamingo_val_t* const toolchain = args->args[0];
	char* const STR_CLEANUP version = strndup_c(args->args[1]->str.str, args->args[1]->str.size);
	int64_t const api_level = args->args[2]->integer.integer;

	// Get SDK path to find NDK within it.

	char* STR_CLEANUP sdk_path = NULL;

	if (get_sdk_path(&sdk_path) < 0) {
		return -1;
	}

	// Build NDK toolchain paths.
	// NDK clang requires a versioned target triple (e.g. aarch64-linux-android31).

	char* STR_CLEANUP ndk_path = NULL;
	asprintf_c(&ndk_path, "%s/ndk/%s", sdk_path, version);

	char* STR_CLEANUP toolchain_path = NULL;
	asprintf_c(&toolchain_path, "%s/toolchains/llvm/prebuilt/linux-x86_64", ndk_path);

	char* STR_CLEANUP cc = NULL;
	asprintf_c(&cc, "%s/bin/aarch64-linux-android%" PRId64 "-clang", toolchain_path, api_level);

	char* STR_CLEANUP cxx = NULL;
	asprintf_c(&cxx, "%s/bin/aarch64-linux-android%" PRId64 "-clang++", toolchain_path, api_level);

	char* STR_CLEANUP ar = NULL;
	asprintf_c(&ar, "%s/bin/llvm-ar", toolchain_path);

	// Check that NDK exists.

	if (access(cc, X_OK) < 0) {
		LOG_FATAL(ANDROID "._get_ndk: NDK %s (API level %" PRId64 ") not found at '%s'. Did you call Android.require(\"ndk/%s\")?", version, api_level, cc, version);
		return -1;
	}

	// Set fields on Toolchain instance.

	flamingo_inst_set(toolchain, "cc", flamingo_val_make_cstr(cc));
	flamingo_inst_set(toolchain, "cxx", flamingo_val_make_cstr(cxx));
	flamingo_inst_set(toolchain, "ar", flamingo_val_make_cstr(ar));

	char* STR_CLEANUP sysroot = NULL;
	asprintf_c(&sysroot, "%s/sysroot", toolchain_path);
	flamingo_inst_set(toolchain, "sysroot", flamingo_val_make_cstr(sysroot));

	char* STR_CLEANUP glue_path = NULL;
	asprintf_c(&glue_path, "%s/sources/android/native_app_glue", ndk_path);
	flamingo_inst_set(toolchain, "native_app_glue_path", flamingo_val_make_cstr(glue_path));

	*rv = flamingo_val_make_none();
	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "require", key_size) == 0) {
		require_val = val;
	}

	else if (flamingo_cstrcmp(key, "_get_ndk", key_size) == 0) {
		get_ndk_val = val;
	}
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == require_val) {
		return require(args, rv);
	}

	else if (callable == get_ndk_val) {
		return get_ndk(args, rv);
	}

	*consumed = false;
	return 0;
}

bob_class_t BOB_CLASS_ANDROID = {
	.name = ANDROID,
	.populate = populate,
	.call = call,
};
