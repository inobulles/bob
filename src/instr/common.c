#include <instr.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define ENV_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#define ENV_PATH "PATH"

void setup_env(char* working_dir) {
	char* const env_lib_path = getenv(ENV_LD_LIBRARY_PATH);
	char* const env_bin_path = getenv(ENV_PATH);

	char* lib_path;
	char* path;

	// format new library & binary search paths

	if (!env_lib_path)
		lib_path = strdup(bin_path);

	else if (asprintf(&lib_path, "%s:%s", env_lib_path, bin_path)) {}

	if (!env_bin_path)
		path = strdup(bin_path);

	else if (asprintf(&path, "%s:%s", env_bin_path, bin_path)) {}

	// set environment variables

	if (setenv(ENV_LD_LIBRARY_PATH, lib_path, true) < 0)
		errx(EXIT_FAILURE, "setenv(\"" ENV_LD_LIBRARY_PATH "\", \"%s\"): %s", lib_path, strerror(errno));

	if (setenv(ENV_PATH, path, true) < 0)
		errx(EXIT_FAILURE, "setenv(\"" ENV_PATH "\", \"%s\"): %s", path, strerror(errno));

	// move into working directory

	if (working_dir && chdir(working_dir) < 0)
		errx(EXIT_FAILURE, "chdir(\"%s\"): %s", working_dir, strerror(errno));
}
