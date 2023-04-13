#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn meta_bind_foreign_method(bool static_, char const* signature);

// methods

void meta_cwd(WrenVM* vm);
void meta_getenv(WrenVM* vm);
void meta_instruction(WrenVM* vm);
void meta_os(WrenVM* vm);
void meta_prefix(WrenVM* vm);
void meta_setenv(WrenVM* vm);
