#pragma once

#include <errno.h>
#include <fts.h>

#include "../util.h"

// methods

static void file_list(WrenVM* vm) {
	CHECK_ARGC("File.list", 2)

	char const* path = wrenGetSlotString(vm, 1);
	double _depth = wrenGetSlotDouble(vm, 2);
	size_t depth = depth; // TODO

	// create return list

	wrenSetSlotNewList(vm, 0);

	// walk through directory

	char* const path_argv[] = { (char*) path, NULL };
	FTS* fts = fts_open(path_argv, 0, NULL);

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

			wrenEnsureSlots(vm, 3 + path_count);
			wrenSetSlotString(vm, 3 + path_count, path);
			wrenInsertInList(vm, 0, -1, 3 + path_count);
		}
	}

	// cleanup

err:

	fts_close(fts);
}

// foreign method binding

static WrenForeignMethodFn file_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "list(_,_)", file_list)

	// unknown

	return unknown_foreign;
}
