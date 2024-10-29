// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <class/class.h>
#include <logging.h>

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>

static flamingo_val_t* list_val = NULL;

static int list(flamingo_arg_list_t* args, flamingo_val_t** rv) {
	assert(args->count == 1);
	assert(args->args[0]->kind == FLAMINGO_VAL_KIND_STR);

	char* const path = strndup(args->args[0]->str.str, args->args[0]->str.size);
	assert(path != NULL);

	// Create a list to store the paths.

	flamingo_val_t* const out_list = flamingo_val_make_none();
	out_list->kind = FLAMINGO_VAL_KIND_VEC;

	// Walk through directory.

	size_t const depth = 0; // XXX Hardcoded for now.

	char* const path_argv[] = {(char*) path, NULL};
	FTS* const fts = fts_open(path_argv, FTS_LOGICAL, NULL);

	if (fts == NULL) {
		LOG_FATAL("fts_open(\"%s\"): %s", path, strerror(errno))
		return -1;
	}

	for (FTSENT* ent; (ent = fts_read(fts));) {
		char* const path = ent->fts_path; // Shadow parent scope's 'path'.

		switch (ent->fts_info) {
		case FTS_DP:

			break; // Ignore directories being visited in postorder.

		case FTS_SL:
		case FTS_SLNONE:

			LOG_ERROR("fts_read: Got 'FTS_SL' or 'FTS_SLNONE' when 'FTS_LOGICAL' was passed");
			break; // Ignore symlinks.

		case FTS_DOT:

			LOG_ERROR("fts_read: Read a '.' or '..' entry, which shouldn't happen as the 'FTS_SEEDOT' option was not passed to 'fts_open'");
			break;

		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:

			LOG_ERROR("fts_read: Failed to read '%s': %s", path, strerror(errno));
			break;

		case FTS_D:
		case FTS_DC:
		case FTS_F:
		case FTS_DEFAULT:
		default:

			if (depth > 0 && (size_t) ent->fts_level > depth) {
				continue;
			}

			flamingo_val_t* const path_val = flamingo_val_make_cstr(path);

			out_list->vec.elems = realloc(out_list->vec.elems, (out_list->vec.count + 1) * sizeof(flamingo_val_t*));
			assert(out_list->vec.elems != NULL);
			out_list->vec.elems[out_list->vec.count++] = path_val;
		}
	}

	fts_close(fts);

	*rv = out_list;

	return 0;
}

static void populate(char* key, size_t key_size, flamingo_val_t* val) {
	if (flamingo_cstrcmp(key, "list", key_size) == 0) {
		list_val = val;
	}
}

static int call(flamingo_val_t* callable, flamingo_arg_list_t* args, flamingo_val_t** rv, bool* consumed) {
	*consumed = true;

	if (callable == list_val) {
		return list(args, rv);
	}

	*consumed = false;
	return 0;
}

bob_class_t const BOB_CLASS_FS = {
	.name = "Fs",
	.populate = populate,
	.call = call,
};
