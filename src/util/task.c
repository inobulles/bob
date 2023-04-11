#include <util.h>

static task_t* tasks = NULL;
static size_t task_count = 0;

static char const* get_verb(task_kind_t kind) {
	if (kind == TASK_KIND_COMPILE)
		return "Compiling";

	return "Running";
}

static char const* get_past(task_kind_t kind) {
	if (kind == TASK_KIND_COMPILE)
		return "compiled";

	return "ran";
}

static char const* get_noun(task_kind_t kind) {
	if (kind == TASK_KIND_COMPILE)
		return "compilation";

	return "task";
}

static bool is_relevant(task_t* task, task_kind_t kind) {
	if (task->completed)
		return false;

	if (task->kind != kind)
		return false;

	return true;
}

task_t* add_task(task_kind_t kind, char* name, exec_args_t* exec_args) {
	tasks = realloc(tasks, ++task_count * sizeof *tasks);
	task_t* const task = &tasks[task_count - 1];

	task->kind = kind;
	task->name = name;
	task->exec_args = exec_args;

	task->pid = execute_async(exec_args);
	return task;
} 

size_t wait_for_tasks(task_kind_t kind) {
	// count relevant tasks

	size_t relevant_count = 0;

	for (size_t i = 0; i < task_count; i++) {
		task_t* const task = &tasks[i];

		if (!is_relevant(task, kind))
			continue;

		relevant_count++;
	}

	// if there are none, return successfully straight away

	if (!relevant_count)
		return 0;

	// otherwise, start a progress bar and wait for each process individually
	// TODO in the future, it would be nice to batch these processes in groups of n at a time
	//      otherwise we're just context switching needlessly, and the user has no control over how many threads to build with

	progress_t* const progress = progress_new();
	size_t err_count = 0;

	for (size_t i_all = 0, i = 0; i_all < task_count; i_all++) {
		task_t* const task = &tasks[i_all];

		if (!is_relevant(task, kind))
			continue;

		char const* const verb = get_verb(task->kind);
		progress_update(progress, i, relevant_count, "%s '%s' (%zu of %zu)", verb, task->name, i + 1, relevant_count);

		task->result = wait_for_process(task->pid);
		err_count += !!task->result;

		i++;
	}

	// print out warning & error messages if there are any

	for (size_t i = 0; i < task_count; i++) {
		task_t* const task = &tasks[i];

		if (!is_relevant(task, kind))
			continue;

		exec_args_t* const exec_args = task->exec_args;
		char const* const verb = get_verb(task->kind);

		// print out stderr of the compilation process
		// we don't only do this on error, because warnings are also printed to stderr

		char* const out = exec_args_read_out(exec_args, PIPE_STDERR);

		if (*out) {
			if (task->result == EXIT_SUCCESS)
				LOG_WARN("%s '%s' succeeded with warnings:", verb, task->name)

			else
				LOG_ERROR("%s '%s' failed with errors:", verb, task->name)

			fprintf(stderr, "%s", out);
		}

		free(out);

		// then, free the task struct and mark as completed

		task->completed = true;

		free(task->name);
		exec_args_del(task->exec_args);
	}

	// print out final message

	char const* const noun = get_noun(kind);
	char const* const past = get_past(kind);

	if (err_count)
		LOG_ERROR("Failed %s", noun)

	else
		LOG_SUCCESS("All %zu source files %s", relevant_count, past)

	return err_count;
}
