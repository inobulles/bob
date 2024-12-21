// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#include <stdio.h>
#include <stdlib.h>

#include <dep2.h>

#if !defined(__DEP2__)
# error "dep2.h not included"
#endif

int main(void) {
	printf("Hello, world! (deps)\n");
	return EXIT_SUCCESS;
}
