#include <umber.h>
#define UMBER_COMPONENT "bob"

#include <wren.h>
#include <stdio.h>

int main(char* argv[], int argc) {
	// XXX for now we're just gonna assume 'bob build' is the only thing being run each time
	// read build configuration file
	// TODO in the future, it'd be nice if this could detect various different scenarios and adapt intelligently, such as not finding a 'build.wren' file but instead finding a 'Makefile'
	
	FILE* fp = fopen("build.wren", "r");

	if (!fp) {
		LOG_FATAL("Couldn't read 'build.wren'!")
		return EXIT_FAILURE;
	}

	fseek(fp, 0, SEEK_END);
	size_t bytes = ftell(fp);
	rewind(fp);

	char* config = malloc(bytes);

	if (fread(config, 1, bytes, fp))
		;

	printf("%s\n", config);

	free(config);

	return EXIT_SUCCESS;
}
