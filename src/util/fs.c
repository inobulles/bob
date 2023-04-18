#include <util.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

size_t file_get_size(FILE* fp) {
	fseek(fp, 0, SEEK_END);
	size_t const size = ftell(fp);
	rewind(fp);

	return size;
}

char* file_read_str(FILE* fp, size_t size) {
	char* const str = malloc(size + 1);

	if (fread(str, 1, size, fp)) {}

	str[size] = 0;
	return str;
}

int path_write_str(char* path, char* str) {
	if (!str)
		return 0;

	FILE* const fp = fopen(path, "wb");

	if (!fp) {
		LOG_ERROR("fopen(\"%s\", wb): %s", path, strerror(errno))
		return -1;
	}

	fprintf(fp, "%s", str);
	fclose(fp);

	return 0;
}

int mkdir_recursive(char const* _path) {
	int rv = -1;

	// we don't need to do anything if path is empty

	if (!*_path)
		return 0;

	char* const __attribute__((cleanup(strfree))) orig_path = strdup(_path);
	char* path = orig_path;

	// remember previous working directory, because to make our lives easier, we'll be jumping around the place to create our subdirectories

	char* const __attribute__((cleanup(strfree))) cwd = getcwd(NULL, 0);

	if (!cwd) {
		LOG_ERROR("getcwd: %s", strerror(errno))
		goto err_cwd;
	}

	// if we're dealing with a path relative to $HOME, chdir to $HOME first

	if (*path == '~') {
		char* const home = getenv("HOME");

		// if $HOME isn't set, treat as an absolute directory

		if (!home)
			*path = '/';

		else if (chdir(home) < 0) {
			LOG_ERROR("chdir($HOME): %s", strerror(errno))
			goto err_home;
		}
	}

	// if we're dealing with an absolute path, chdir to '/' and treat path as relative

	if (*path == '/' && chdir("/") < 0) {
		LOG_ERROR("chdir(\"/\"): %s", strerror(errno))
		goto err_abs;
	}

	// parse the path itself

	char* bit;

	while ((bit = strsep(&path, "/"))) {
		// ignore if the bit is empty

		if (!bit || !*bit)
			continue;

		// ignore if the bit refers to the current directory

		if (!strcmp(bit, "."))
			continue;

		// don't attempt to mkdir if we're going backwards, only chdir

		if (!strcmp(bit, ".."))
			goto no_mkdir;

		if (mkdir(bit, 0755) < 0 && errno != EEXIST) {
			LOG_ERROR("mkdir(\"%s\"): %s", bit, strerror(errno))
			goto err_mkdir;
		}

	no_mkdir:

		if (chdir(bit) < 0) {
			LOG_ERROR("chdir(\"%s\"): %s", bit, strerror(errno))
			goto err_chdir;
		}
	}

	// success

	rv = 0;

err_chdir:
err_mkdir:

	// move back to current directory once we're sure the output directory exists (or there's an error)

	if (chdir(cwd) < 0)
		LOG_ERROR("chdir(\"%s\"): %s", cwd, strerror(errno))

err_abs:
err_home:
err_cwd:

	return rv;
}

int copy_recursive(char const* _src, char const* dest) {
	// it's unfortunate, but to be as cross-platform as possible, we must shell out execution to the 'cp' binary
	// would've loved to use libcopyfile but, alas, POSIX is missing features :(
	// if 'src' is a directory, append a slash to it to override stupid cp(1) behaviour

	struct stat sb;

	if (stat(_src, &sb) < 0) {
		LOG_ERROR("stat(\"%s\"): %s", _src, strerror(errno))
		return EXIT_FAILURE;
	}

	bool const add_slash = S_ISDIR(sb.st_mode);
	char* src = (void*) _src;

	if (add_slash)
		if (asprintf(&src, "%s/", src)) {};

	exec_args_t* exec_args = exec_args_new(4, "cp", "-RpP", src, dest);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	if (add_slash) 
		free(src);

	return rv;
}

int remove_recursive(char const* path) {
	// same comment as for 'copy'

	exec_args_t* exec_args = exec_args_new(3, "rm", "-rf", path);
	int rv = execute(exec_args);
	exec_args_del(exec_args);

	return rv;
}
