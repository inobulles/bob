// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <task.h>

#include <assert.h>
#include <stdlib.h>

static void* worker(void* data) {
	pool_t* const pool = data;

	for (;;) {
		// We must lock on task iteration to prevent race conditions.

		pthread_mutex_lock(&pool->lock);
		task_t* task = NULL;

		for (size_t i = 0; i < pool->task_count; i++) {
			task = &pool->tasks[i];

			if (!task->done) {
				goto found;
			}
		}

		// No more tasks left to run!

		pthread_mutex_unlock(&pool->lock);
		break;

found:

		// Run found task.
		// Get the stuff inside of the task pointer because it may move due to a realloc once we release the lock.

		task->done = true;

		task_fn_t const fn = task->fn;
		void* const data = task->data;

		pthread_mutex_unlock(&pool->lock);

		if (!fn(data)) {
			continue;
		}

		// There was an error; re-acquire lock and cancel all threads.

		pthread_mutex_lock(&pool->lock);
		pool->error = true;

		for (size_t i = 0; i < pool->task_count; i++) {
			pthread_cancel(pool->workers[i]); // TODO What happens with the heap memory here?
		}

		pthread_mutex_unlock(&pool->lock);
	}

	return NULL;
}

void pool_init(pool_t* pool, size_t worker_count) {
	pthread_mutex_init(&pool->lock, NULL);
	pthread_mutex_lock(&pool->lock);

	pool->error = false;

	pool->worker_count = worker_count;
	pool->workers = malloc(worker_count * sizeof(pthread_t));
	assert(pool->workers != NULL);

	for (size_t i = 0; i < worker_count; i++) {
		pthread_create(&pool->workers[i], NULL, worker, pool);
	}

	pool->task_count = 0;
	pool->tasks = NULL;

	pthread_mutex_unlock(&pool->lock);
}

void pool_free(pool_t* pool) {
	if (pool->tasks != NULL) {
		free(pool->tasks);
	}

	for (size_t i = 0; i < pool->worker_count; i++) {
		pthread_cancel(pool->workers[i]);
	}

	free(pool->workers);
}

void pool_add_task(pool_t* pool, task_fn_t cb, void* data) {
	pthread_mutex_lock(&pool->lock);

	pool->tasks = realloc(pool->tasks, (pool->task_count + 1) * sizeof *pool->tasks);
	assert(pool->tasks != NULL);

	task_t* const task = &pool->tasks[pool->task_count++];

	task->done = false;
	task->fn = cb;
	task->data = data;

	pthread_mutex_unlock(&pool->lock);
}

bool pool_wait(pool_t* pool) {
	for (size_t i = 0; i < pool->worker_count; i++) {
		pthread_t* const worker = &pool->workers[i];
		pthread_join(*worker, NULL);
	}

	return pool->error;
}
