// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2025 Aymeric Wibo

#include <common.h>

#include <class/class.h>
#include <logging.h>
#include <str.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static flamingo_val_t* os_val = NULL;
static flamingo_val_t* getenv_val = NULL;

static int os(flamingo_val_t** rv) {
	struct utsname utsname;

	if (uname(&utsname) < 0) {
		LOG_FATAL("uname: %s", strerror(errno));
		return -1;
	}

	*rv = flamingo_val_make_str(strlen(utsname.sysname), utsname.sysname);
	return 0;
}

static int bob_getenv(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 1);
	assert(args->args[0]->kind == FLAMINGO_VAL_KIND_STR);

	char* const STR_CLEANUP key = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(key != NULL);

	char* val = getenv(key);

	if (val == NULL) {
		*rv = flamingo_val_make_none();
	}

	else {
		*rv = flamingo_val_make_cstr(val);
	}

	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "os", key_size) == 0) {
		os_val = val;
	}

	else if (flamingo_cstrcmp(key, "getenv", key_size) == 0) {
		getenv_val = val;
	}
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == os_val) {
		assert(args->count == 0);
		return os(rv);
	}

	if (callable == getenv_val) {
		return bob_getenv(args, rv);
	}

	*consumed = false;
	return 0;
}

bob_class_t BOB_CLASS_PLATFORM = {
	.name = "Platform",
	.populate = populate,
	.call = call,
};
