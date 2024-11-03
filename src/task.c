// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <task.h>

#include <assert.h>
#include <stdlib.h>

static void* worker(void* data) {
	pool_t* const pool = data;

	while (!pool->error) {
		// We must lock on task iteration to prevent race conditions.

		pthread_mutex_lock(&pool->lock);
		task_t* task = NULL;

		for (size_t i = 0; i < pool->task_count; i++) {
			task = &pool->tasks[i];

			if (!task->started) {
				goto found;
			}
		}

		// No more tasks left to run!

		pthread_mutex_unlock(&pool->lock);
		break;

found:

		// Run found task.
		// Get the stuff inside of the task pointer because it may move due to a realloc once we release the lock.

		task->started = true;

		task_fn_t const fn = task->fn;
		void* const data = task->data;

		pthread_mutex_unlock(&pool->lock);

		if (!fn(data)) {
			continue;
		}

		// We were asked to stop; cancel all workers.
		// We can't cancel them directly because they might still hold the logging lock, and locking a mutex is not a cancellation point.
		// Instead, rely on each worker to stop when 'pool->error' is set.

		pool->error = true;
	}

	return NULL;
}

void pool_init(pool_t* pool, size_t worker_count) {
	pthread_mutex_init(&pool->lock, NULL);
	pthread_mutex_lock(&pool->lock);

	pool->error = false;
	pool->started = false;

	pool->worker_count = worker_count;
	pool->workers = malloc(worker_count * sizeof(pthread_t));
	assert(pool->workers != NULL);

	pool->task_count = 0;
	pool->tasks = NULL;

	pthread_mutex_unlock(&pool->lock);
}

void pool_free(pool_t* pool) {
	pool->error = true; // Make sure we end as quickly as possible.
	pool_wait(pool);

	free(pool->workers);

	if (pool->tasks != NULL) {
		free(pool->tasks);
	}
}

void pool_add_task(pool_t* pool, task_fn_t cb, void* data) {
	pthread_mutex_lock(&pool->lock);

	pool->tasks = realloc(pool->tasks, (pool->task_count + 1) * sizeof *pool->tasks);
	assert(pool->tasks != NULL);

	task_t* const task = &pool->tasks[pool->task_count++];

	task->started = false;
	task->fn = cb;
	task->data = data;

	pthread_mutex_unlock(&pool->lock);
}

int pool_start(pool_t* pool) {
	if (pool->started == true) {
		return 0;
	}

	for (size_t i = 0; i < pool->worker_count; i++) {
		pthread_create(&pool->workers[i], NULL, worker, pool);
	}

	pool->started = true;
	return 0;
}

int pool_wait(pool_t* pool) {
	pool_start(pool);

	for (size_t i = 0; i < pool->worker_count; i++) {
		pthread_t* const worker = &pool->workers[i];
		pthread_join(*worker, NULL);
	}

	return pool->error ? -1 : 0;
}
