#include <util.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

void strfree(char* const* str_ref) {
	char* const str = *str_ref;

	if (!str) {
		return;
	}

	free(str);
}

uint64_t hash_str(char const* str) { // djb2 algorithm
	uint64_t hash = 5381;
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++) {
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]
	}

	return hash;
}

void navigate_project_path(void) {
	// navigate into project directory, if one was specified

	if (project_path && chdir(project_path) < 0) {
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", project_path, strerror(errno));
	}
}

void ensure_out_path(void) {
	// make sure output directory exists - create it if it doesn't

	if (mkdir_recursive(rel_bin_path) < 0) {
		errx(EXIT_FAILURE, "mkdir_recursive(\"%s\"): %s", rel_bin_path, strerror(errno));
	}

	// get absolute path of output directory so we don't ever get lost or confused

	bin_path = realpath(rel_bin_path, NULL);

	if (!bin_path) {
		errx(EXIT_FAILURE, "realpath(\"%s\"): %s", rel_bin_path, strerror(errno));
	}
}

void fix_out_path_owner(void) {
	// get the owner of the parent directory to the output path
	// we want the output path to recursively have the same permissions as it

	char* CLEANUP_STR parent = NULL;
	if (asprintf(&parent, "%s/..", bin_path)) {}

	char* const path_argv[] = { (char*) parent, NULL };
	FTS* const fts = fts_open(path_argv, FTS_PHYSICAL | FTS_XDEV, NULL);

	if (fts == NULL) {
		errx(EXIT_FAILURE, "fts_open(\"%s\"): %s", parent, strerror(errno));
	}

	struct stat sb;

	if (stat(parent, &sb) < 0) {
		errx(EXIT_FAILURE, "stat(\"%s\"): %s", parent, strerror(errno));
	}

	uid_t const uid = sb.st_uid;
	gid_t const gid = sb.st_gid;

	for (FTSENT* ent; (ent = fts_read(fts));) {
		char* const path = ent->fts_path; // shadow parent scope's 'path'

		switch (ent->fts_info) {
		case FTS_DP:

			break; // ignore directories being visited in postorder

		case FTS_DOT:

			LOG_ERROR("fts_read: Read a '.' or '..' entry, which shouldn't happen as the 'FTS_SEEDOT' option was not passed to 'fts_open'")
			break;

		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:

			LOG_ERROR("fts_read: Failed to read '%s': %s", path, strerror(errno))
			break;

		case FTS_SL:
		case FTS_SLNONE:
		case FTS_D:
		case FTS_DC:
		case FTS_F:
		case FTS_DEFAULT:
		default:

			if (chown(path, uid, gid) < 0) {
				LOG_WARN("chown(\"%s\", %d, %d): %s", path, uid, gid, strerror(errno))
			}
		}
	}

	fts_close(fts);
}

char const* install_prefix(void) {
	if (prefix) {
		return prefix;
	}

	// if on FreeBSD/aquaBSD (and, to be safe, anywhere else), the prefix will be '/usr/local'
	// on Linux, it will simply be '/usr'

#if defined(__linux__)
	return "/usr";
#endif

	return "/usr/local";
}
