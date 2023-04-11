#pragma once

#include <util.h>

// foreign method binding

WrenForeignMethodFn resources_bind_foreign_method(bool static_, char const* signature);

// methods

void resources_install(WrenVM* vm);
