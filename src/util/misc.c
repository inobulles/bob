#include <util.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

void strfree(char* const* str_ref) {
	char* const str = *str_ref;

	if (!str)
		return;

	free(str);
}

uint64_t hash_str(char const* str) { // djb2 algorithm
	uint64_t hash = 5381;
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++)
		hash = ((hash << 5) + hash) + str[i]; // hash * 33 + ptr[i]

	return hash;
}

void navigate_project_path(void) {
	// navigate into project directory, if one was specified

	if (project_path && chdir(project_path) < 0)
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", project_path, strerror(errno));
}

void ensure_out_path(void) {
	// make sure output directory exists - create it if it doesn't

	if (mkdir_recursive(rel_bin_path) < 0)
		errx(EXIT_FAILURE, "mkdir_recursive(\"%s\"): %s", rel_bin_path, strerror(errno));

	// get absolute path of output directory so we don't ever get lost or confused

	bin_path = realpath(rel_bin_path, NULL);

	if (!bin_path)
		errx(EXIT_FAILURE, "realpath(\"%s\"): %s", rel_bin_path, strerror(errno));
}

char const* install_prefix(void) {
	if (prefix)
		return prefix;

	// if on FreeBSD/aquaBSD (and, to be safe, anywhere else), the prefix will be '/usr/local'
	// on Linux, it will simply be '/usr'

#if defined(__linux__)
	return "/usr";
#endif

	return "/usr/local";
}
