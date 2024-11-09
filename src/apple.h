// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#if defined(__APPLE__)
int apple_set_install_id(char const* path, char const* id, char** err);
#endif
