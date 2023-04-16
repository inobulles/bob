#pragma once

#include <common.h>

#include <stdio.h>

#include <util.h>
#include <wren.h>

// macros

#define INSTALL_MAP "install"
#define PACKAGE_MAP "packages"

// common stuff between instructions

void setup_env(char* working_dir);

// actual instructions

int do_build(void);
int do_run(int argc, char** argv);
int do_install(void);
int do_skeleton(int argc, char** argv);
int do_test(void);
int do_package(int argc, char** argv);
