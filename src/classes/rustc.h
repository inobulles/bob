// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn rustc_bind_foreign_method(bool static_, char const* signature);

// constructor/destructor

void rustc_new(WrenVM* vm);
void rustc_del(void* _rustc);

// getters

void rustc_get_path(WrenVM* vm);

// setters

void rustc_set_path(WrenVM* vm);

// methods

void rustc_add_dep(WrenVM* vm);
void rustc_compile(WrenVM* vm);
