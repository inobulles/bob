#pragma once

#include <errno.h>
#include <fts.h>

#include "../util.h"

// methods

static void file_list(WrenVM* vm) {
	CHECK_ARGC("File.list", 2, 2)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)
	ASSERT_ARG_TYPE(2, WREN_TYPE_NUM)

	char const* const path = wrenGetSlotString(vm, 1);
	double const _depth = wrenGetSlotDouble(vm, 2);
	size_t const depth = _depth;

	// create return list

	wrenEnsureSlots(vm, 3);
	wrenSetSlotNewList(vm, 0);

	// walk through directory

	char* const path_argv[] = { (char*) path, NULL };
	FTS* const fts = fts_open(path_argv, 0, NULL);

	if (!fts) {
		// TODO errors like this should stop execution of wren (same thing in 'util.h' macros & 'cc.h')

		LOG_ERROR("fts_open(\"%s\"): %s", path, strerror(errno))
		goto err;
	}

	size_t path_count = 0;

	for (FTSENT* ent; (ent = fts_read(fts));) {
		char* const path = ent->fts_path; // shadow parent scope's 'path'

		switch (ent->fts_info) {
		case FTS_D:
		case FTS_DC:
		case FTS_DP:

			break; // ignore directories

		case FTS_SL:
		case FTS_SLNONE:

			break; // ignore symlinks

		case FTS_DOT:

			LOG_WARN("fts_read: Read a '.' or '..' entry, which shouldn't happen as the 'FTS_SEEDOT' option was not passed to 'fts_open'")
			break;

		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:

			LOG_ERROR("fts_read: Failed to read '%s': %s", path, strerror(errno))
			break;

		case FTS_F:
		case FTS_DEFAULT:
		default:

			if (depth > 0 && ent->fts_level > depth) {
				continue;
			}

			wrenSetSlotString(vm, 3, path);
			wrenInsertInList(vm, 0, -1, 3);
		}
	}

	// cleanup

err:

	fts_close(fts);
}

static void file_exec(WrenVM* vm) {
	CHECK_ARGC("File.exec", 1, 2)
	bool const has_args = argc == 2;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	if (has_args) {
		ASSERT_ARG_TYPE(2, WREN_TYPE_LIST)
	}

	char const* const path = wrenGetSlotString(vm, 1);
	size_t const args_list_len = has_args ? wrenGetListCount(vm, 2) : 0;

	// actually execute file

	wrenEnsureSlots(vm, 3); // we just need a single extra slot for each list element
	exec_args_t* exec_args = exec_args_new(1, path);

	// add list of arguments to exec_args if we have them

	for (size_t i = 0; has_args && i < args_list_len; i++) {
		wrenGetListElement(vm, 2, i, 3);

		if (wrenGetSlotType(vm, 3) != WREN_TYPE_STRING) {
			LOG_WARN("'File.exec' list element %d of argument 2 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const arg = wrenGetSlotString(vm, 3);
		exec_args_add(exec_args, arg);
	}

	// actually execute file

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		LOG_WARN("'File.exec' failed execution with error code %d - here is the exec_args struct:", rv)
		exec_args_print(exec_args);
	}

	exec_args_del(exec_args);
	wrenSetSlotDouble(vm, 0, rv);
}

// foreign method binding

static WrenForeignMethodFn file_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "list(_,_)", file_list)
	BIND_FOREIGN_METHOD(true, "exec(_)", file_exec)
	BIND_FOREIGN_METHOD(true, "exec(_,_)", file_exec)

	// unknown

	return unknown_foreign;
}
