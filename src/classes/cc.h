// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Aymeric Wibo

#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn cc_bind_foreign_method(bool static_, char const* signature);

// constructor/destructor

void cc_new(WrenVM* vm);
void cc_del(void* _cc);

// getters

void cc_get_path(WrenVM* vm);

// setters

void cc_set_path(WrenVM* vm);

// methods

void cc_add_lib(WrenVM* vm);
void cc_add_opt(WrenVM* vm);
void cc_compile(WrenVM* vm);
