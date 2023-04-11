#pragma once

#include <util.h>

#include <err.h>
#include <unistd.h>

#include <sys/utsname.h>

// helpers

static void meta_os_add(WrenVM* vm, char const* str) {
	wrenSetSlotString(vm, 1, str);
	wrenInsertInList(vm, 0, -1, 1);
}

// methods

static void meta_cwd(WrenVM* vm) {
	CHECK_ARGC("Meta.cwd", 0, 0)

	// return current working directory

	char* const cwd = getcwd(NULL, 0);
	wrenSetSlotString(vm, 0, cwd);

	free(cwd);
}

static void meta_getenv(WrenVM* vm) {
	CHECK_ARGC("Meta.getenv", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	char const* const env = wrenGetSlotString(vm, 1);

	// return contents of environment variable if it exists, null if not

	char const* const contents = getenv(env);

	if (contents) {
		wrenSetSlotString(vm, 0, contents);
	}
}

static void meta_instruction(WrenVM* vm) {
	CHECK_ARGC("Meta.instruction", 0, 0)

	// return the current instruction

	wrenEnsureSlots(vm, 1);
	wrenSetSlotString(vm, 0, curr_instr);
}

static void meta_os(WrenVM* vm) {
	CHECK_ARGC("Meta.os", 0, 0)

	// create return list

	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);

	// set runtime OS information
	// this info is cached

	static struct utsname name;
	static bool cached = false;

	if (cached) {
		goto cached;
	}

	if (!cached && uname(&name) < 0) {
		errx(EXIT_FAILURE, "uname: %s", strerror(errno));
	}

	// check if running under WSL (only on Linux)
	// I actually hate this, how tf is there no surefire way of detecting if running under WSL
	// how can you be so utterly incompetent at literally everything you do Microsoft
	// https://github.com/microsoft/WSL/issues/4071

#if defined(__linux__)
	static bool wsl = false;

	// first, check the release name information

	wsl |= !!strstr(name.release, "Microsoft"); // likely WSL1
	wsl |= !!strstr(name.release, "microsoft"); // likely WSL2

	// then, idk, check for these environment variables

	wsl |= !!getenv("WSL_INTEROP");
	wsl |= !!getenv("WSL_DISTRO_NAME");
#endif

	// all of this is cached so we don't have to call 'uname' as often

	cached = true;

cached:

	meta_os_add(vm, name.sysname);

#if defined(__linux__)
	if (wsl) {
		meta_os_add(vm, "WSL");
	}
#endif
}

static void meta_prefix(WrenVM* vm) {
	CHECK_ARGC("Meta.prefix", 0, 0)
	wrenSetSlotString(vm, 0, install_prefix());
}

static void meta_setenv(WrenVM* vm) {
	CHECK_ARGC("Meta.setenv", 1, 2)
	bool const unset = argc == 1;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	if (!unset) {
		ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)
	}

	char const* const env = wrenGetSlotString(vm, 1);
	char const* const val = unset ? NULL : wrenGetSlotString(vm, 2);

	// set/unset environment variable

	if (unset) {
		if (unsetenv(env) < 0) {
			LOG_WARN("Meta.setenv: unsetenv(\"%s\"): %s", env, strerror(errno))
		}
	}

	else {
		if (setenv(env, val, true) < 0) {
			LOG_WARN("Meta.setenv: setenv(\"%s\", \"%s\"): %s", env, val, strerror(errno))
		}
	}
}

// foreign method binding

static WrenForeignMethodFn meta_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "cwd()", meta_cwd)
	BIND_FOREIGN_METHOD(true, "getenv(_)", meta_getenv)
	BIND_FOREIGN_METHOD(true, "instruction()", meta_instruction)
	BIND_FOREIGN_METHOD(true, "os()", meta_os)
	BIND_FOREIGN_METHOD(true, "prefix()", meta_prefix)
	BIND_FOREIGN_METHOD(true, "setenv(_)", meta_setenv)
	BIND_FOREIGN_METHOD(true, "setenv(_,_)", meta_setenv)

	// unknown

	return unknown_foreign;
}
