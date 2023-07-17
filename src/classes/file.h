// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn file_bind_foreign_method(bool static_, char const* signature);

// methods

void file_bob(WrenVM* vm);
void file_chmod(WrenVM* vm);
void file_chown(WrenVM* vm);
void file_exec(WrenVM* vm);
void file_list(WrenVM* vm);
void file_read(WrenVM* vm);
void file_write(WrenVM* vm);
