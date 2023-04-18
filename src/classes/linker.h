#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn linker_bind_foreign_method(bool static_, char const* signature);

// constructor/destructor

void linker_new(WrenVM* vm);
void linker_del(void* _linker);

// getters

void linker_get_path(WrenVM* vm);
void linker_get_archiver_path(WrenVM* vm);

// setters

void linker_set_path(WrenVM* vm);
void linker_set_archiver_path(WrenVM* vm);

// methods

void linker_add_opt(WrenVM* vm);
void linker_add_lib(WrenVM* vm);

void linker_link(WrenVM* vm);
void linker_archive(WrenVM* vm);
