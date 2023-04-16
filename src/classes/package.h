#pragma once

#include <wren.h>

// foreign method binding

WrenForeignMethodFn package_bind_foreign_method(bool static_, char const* signature);

// constructor/destructor

void package_new(WrenVM* vm);
void package_del(void* _package);

// getters

void package_get_name(WrenVM* vm);
void package_get_description(WrenVM* vm);
void package_get_version(WrenVM* vm);
void package_get_author(WrenVM* vm);
void package_get_organization(WrenVM* vm);
void package_get_www(WrenVM* vm);

// setters

void package_set_name(WrenVM* vm);
void package_set_description(WrenVM* vm);
void package_set_version(WrenVM* vm);
void package_set_author(WrenVM* vm);
void package_set_organization(WrenVM* vm);
void package_set_www(WrenVM* vm);
