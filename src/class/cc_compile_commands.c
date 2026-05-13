// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

#include <common.h>

#include <class/cc_compile_commands.h>
#include <logging.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_PATH "compile_commands.json"

_Bool gen_compile_commands = false;

typedef struct {
	char* file;
	char* directory;
	char** args;
	size_t count;
} entry_t;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static entry_t* entries = NULL;
static size_t entry_count = 0;

void cc_compile_commands_add(char const* file, char const* directory, char* const* args, size_t count) {
	pthread_mutex_lock(&lock);

	entries = realloc(entries, (entry_count + 1) * sizeof *entries);
	assert(entries != NULL);

	entry_t* const entry = &entries[entry_count++];

	entry->file = strdup(file);
	entry->directory = strdup(directory);
	entry->count = count;
	entry->args = malloc(count * sizeof *entry->args);
	assert(entry->args != NULL);

	for (size_t i = 0; i < count; i++) {
		entry->args[i] = strdup(args[i]);
	}

	pthread_mutex_unlock(&lock);
}

static void json_str(FILE* f, char const* s) {
	fputc('"', f);

	for (; *s; s++) {
		if (*s == '"' || *s == '\\') {
			fputc('\\', f);
		}

		fputc(*s, f);
	}

	fputc('"', f);
}

int cc_compile_commands_write(void) {
	FILE* const f = fopen(OUTPUT_PATH, "w");

	if (f == NULL) {
		LOG_ERROR("fopen(\"" OUTPUT_PATH "\"): %s", strerror(errno));
		return -1;
	}

	fputs("[\n", f);

	for (size_t i = 0; i < entry_count; i++) {
		entry_t const* const e = &entries[i];

		fputs("  {\n", f);
		fputs("    \"directory\": ", f);
		json_str(f, e->directory);
		fputs(",\n", f);
		fputs("    \"file\": ", f);
		json_str(f, e->file);
		fputs(",\n", f);
		fputs("    \"arguments\": [", f);

		for (size_t j = 0; j < e->count; j++) {
			json_str(f, e->args[j]);

			if (j + 1 < e->count) {
				fputs(", ", f);
			}
		}

		fputs("]\n", f);
		fputs("  }", f);

		if (i + 1 < entry_count) {
			fputc(',', f);
		}

		fputc('\n', f);

		// Free as we go.

		free(e->file);
		free(e->directory);

		for (size_t j = 0; j < e->count; j++) {
			free(e->args[j]);
		}

		free(e->args);
	}

	fputs("]\n", f);
	fclose(f);

	free(entries);
	entries = NULL;
	entry_count = 0;

	LOG_SUCCESS("Generated " OUTPUT_PATH ".");
	return 0;
}
