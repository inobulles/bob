#include <util.h>
#include <classes/file.h>

#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// foreign method binding

WrenForeignMethodFn file_bind_foreign_method(bool static_, char const* signature) {
	// methods

	BIND_FOREIGN_METHOD(true, "bob(_,_)", file_bob)
	BIND_FOREIGN_METHOD(true, "chmod(_,_,_)", file_chmod)
	BIND_FOREIGN_METHOD(true, "chown(_,_)", file_chown)
	BIND_FOREIGN_METHOD(true, "chown(_,_,_)", file_chown)
	BIND_FOREIGN_METHOD(true, "exec(_)", file_exec)
	BIND_FOREIGN_METHOD(true, "exec(_,_)", file_exec)
	BIND_FOREIGN_METHOD(true, "list(_,_)", file_list)
	BIND_FOREIGN_METHOD(true, "read(_)", file_read)
	BIND_FOREIGN_METHOD(true, "write(_,_)", file_write)

	// unknown

	return wren_unknown_foreign;
}

// methods

void file_bob(WrenVM* vm) {
	CHECK_ARGC("File.bob", 2, 2)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)
	ASSERT_ARG_TYPE(2, WREN_TYPE_LIST)

	char const* const path = wrenGetSlotString(vm, 1);
	size_t const args_list_len = wrenGetListCount(vm, 2);

	// actually execute bob

	wrenEnsureSlots(vm, 3); // we just need a single extra slot for each list element
	exec_args_t* exec_args = exec_args_new(7, init_name, "-C", path, "-o", bin_path, "-p", install_prefix());

	// add list of arguments to exec_args if we have them

	for (size_t i = 0; i < args_list_len; i++) {
		wrenGetListElement(vm, 2, i, 3);

		if (wrenGetSlotType(vm, 3) != WREN_TYPE_STRING) {
			LOG_WARN("'File.bob' list element %zu of argument 2 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const arg = wrenGetSlotString(vm, 3);
		exec_args_add(exec_args, arg);
	}

	// actually execute file

	int const rv = execute(exec_args);

	if (rv != EXIT_SUCCESS)
		LOG_WARN("'File.bob' failed execution with error code %d", rv)

	exec_args_del(exec_args);
	wrenSetSlotDouble(vm, 0, rv);
}

void file_chmod(WrenVM* vm) {
	CHECK_ARGC("File.chmod", 3, 3)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)
	ASSERT_ARG_TYPE(2, WREN_TYPE_NUM)
	ASSERT_ARG_TYPE(3, WREN_TYPE_NUM)

	char const* const path = wrenGetSlotString(vm, 1);

	double const _mask = wrenGetSlotDouble(vm, 2);
	mode_t const mask = _mask;

	double const _bit = wrenGetSlotDouble(vm, 3);
	mode_t const bit = _bit;

	// get initial permissions first

	struct stat sb;

	if (stat(path, &sb) < 0) {
		LOG_WARN("'File.chmod' can't change permissions of '%s': stat: %s", path, strerror(errno))
		return;
	}

	mode_t mode = sb.st_mode;

	// find new mode

	mode_t const EXTRA = 07000;
	mode_t const OWNER = 00700;
	mode_t const GROUP = 00070;
	mode_t const OTHER = 00007;

	if (mask & EXTRA) {
		mode &= ~EXTRA;
		mode |= (bit << 9) & EXTRA;
	}

	if (mask & OWNER) {
		mode &= ~OWNER;
		mode |= (bit << 6) & OWNER;
	}

	if (mask & GROUP) {
		mode &= ~GROUP;
		mode |= (bit << 3) & GROUP;
	}

	if (mask & OTHER) {
		mode &= ~OTHER;
		mode |= (bit << 0) & OTHER;
	}

	// actually chmod

	if (chmod(path, mode) < 0) {
		LOG_WARN("'File.chmod' can't change permissions of '%s': chmod: %s", path, strerror(errno))
		return;
	}
}

void file_chown(WrenVM* vm) {
	CHECK_ARGC("File.chown", 2, 3)
	bool const has_group = argc == 3;

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)
	ASSERT_ARG_TYPE(2, WREN_TYPE_STRING)

	if (has_group) {
		ASSERT_ARG_TYPE(3, WREN_TYPE_STRING)
	}

	char const* const path = wrenGetSlotString(vm, 1);
	char const* const username = wrenGetSlotString(vm, 2);
	char const* const groupname = has_group ? wrenGetSlotString(vm, 3) : NULL;

	// get uid from username
	// also get gid in the case we weren't passed a groupname

	struct passwd* passwd = getpwnam(username);

	if (!passwd) {
		LOG_WARN("'File.chown' can't get UID from username '%s': getpwnam: %s", username, strerror(errno))
		return;
	}

	uid_t const uid = passwd->pw_uid;
	gid_t gid = passwd->pw_gid;

	// get gid from groupname, if we have one

	if (groupname) {
		struct group* group = getgrnam(groupname);

		if (!group) {
			LOG_WARN("'File.chown' can't get GID from groupname '%s': getgrnam: %s", groupname, strerror(errno))
			return;
		}

		gid = group->gr_gid;
	}

	// attempt to change ownership

	if (chown(path, uid, gid) < 0) {
		LOG_WARN("'File.chown' can't change ownership of '%s' to '%d:%d': chown: %s", path, uid, gid, strerror(errno))
		return;
	}
}

void file_exec(WrenVM* vm) {
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
			LOG_WARN("'File.exec' list element %zu of argument 2 is of incorrect type (expected 'WREN_TYPE_STRING') - skipping", i)
			continue;
		}

		char const* const arg = wrenGetSlotString(vm, 3);
		exec_args_add(exec_args, arg);
	}

	// actually execute file

	int rv = execute(exec_args);

	if (rv != EXIT_SUCCESS) {
		LOG_WARN("'File.exec' failed execution with error code %d", rv)
	}

	exec_args_del(exec_args);
	wrenSetSlotDouble(vm, 0, rv);
}

void file_list(WrenVM* vm) {
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
	FTS* const fts = fts_open(path_argv, FTS_LOGICAL, NULL);

	if (fts == NULL) {
		LOG_ERROR("fts_open(\"%s\"): %s", path, strerror(errno))
		return;
	}

	for (FTSENT* ent; (ent = fts_read(fts));) {
		char* const path = ent->fts_path; // shadow parent scope's 'path'

		switch (ent->fts_info) {
		case FTS_DP:

			break; // ignore directories being visited in postorder

		case FTS_SL:
		case FTS_SLNONE:

			LOG_ERROR("fts_read: Got 'FTS_SL' or 'FTS_SLNONE' when 'FTS_LOGICAL' was passed");
			break; // ignore symlinks

		case FTS_DOT:

			LOG_ERROR("fts_read: Read a '.' or '..' entry, which shouldn't happen as the 'FTS_SEEDOT' option was not passed to 'fts_open'")
			break;

		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:

			LOG_ERROR("fts_read: Failed to read '%s': %s", path, strerror(errno))

			wrenSetSlotNull(vm, 0);
			goto err;

		case FTS_D:
		case FTS_DC:
		case FTS_F:
		case FTS_DEFAULT:
		default:

			if (depth > 0 && (size_t) ent->fts_level > depth) {
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

void file_read(WrenVM* vm) {
	CHECK_ARGC("File.read", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	char const* const path = wrenGetSlotString(vm, 1);

	// read file & return contents as string

	FILE* fp = fopen(path, "r");

	if (!fp) {
		LOG_WARN("Failed to open file \"%s\" for reading (%s)", path, strerror(errno))
		return;
	}

	size_t const size = file_get_size(fp);
	char* const str = file_read_str(fp, size);

	fclose(fp);

	wrenSetSlotString(vm, 0, str);
	free(str);
}

void file_write(WrenVM* vm) {
	CHECK_ARGC("File.write", 2, 2)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING);
	ASSERT_ARG_TYPE(2, WREN_TYPE_STRING);

	char const* const path = wrenGetSlotString(vm, 1);
	char const* const str = wrenGetSlotString(vm, 2);

	// write to file, simply

	FILE* fp = fopen(path, "w");

	if (!fp) {
		LOG_WARN("Failed to open file \"%s\" for writing (%s)", path, strerror(errno))
		return;
	}

	fprintf(fp, "%s", str);
	fclose(fp);
}
