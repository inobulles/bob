#pragma once

#include <sys/utsname.h>

// helpers

static void meta_os_add(WrenVM* vm, char const* str) {
	wrenSetSlotString(vm, 1, str);
	wrenInsertInList(vm, 0, -1, 1);
}

// methods

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

	// if on FreeBSD/aquaBSD (and, to be safe, anywhere else), the prefix will be '/usr/local'
	// on Linux, it will simply be '/usr'

#if defined(__linux__)
	wrenSetSlotString(vm, 0, "/usr");
#else
	wrenSetSlotString(vm, 0, "/usr/local");
#endif
}

// foreign method binding

static WrenForeignMethodFn meta_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "os()", meta_os)
	BIND_FOREIGN_METHOD(true, "prefix()", meta_prefix)

	// unknown

	return unknown_foreign;
}
