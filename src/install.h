// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include <flamingo/flamingo.h>

int setup_install_map(flamingo_t* flamingo, char const* prefix);
int install_all(char const* prefix);
int install_cookie(char* cookie, bool dylib);
