// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <pthread.h>
#include <stdbool.h>

typedef bool (*task_fn_t)(void* data);

typedef struct {
	bool started;
	task_fn_t fn;
	void* data;
} task_t;

typedef struct {
	pthread_mutex_t lock;

	bool error;
	bool started;

	// (Lazy) task queue.

	size_t task_count;
	task_t* tasks;

	// Worker pool.

	size_t worker_count;
	pthread_t* workers;
} pool_t;

void pool_init(pool_t* pool, size_t worker_count);
void pool_free(pool_t* pool);

void pool_add_task(pool_t* pool, task_fn_t cb, void* data);
int pool_start(pool_t* pool);
int pool_wait(pool_t* pool);
