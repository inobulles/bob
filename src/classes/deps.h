// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <wren.h>

WrenForeignMethodFn deps_bind_foreign_method(bool static_, char const* signature);

// methods

void deps_git(WrenVM* vm);
