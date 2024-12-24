#!/bin/sh
set -e

. tests/common.sh

# Regression test for the following PR:
# https://github.com/inobulles/bob/pull/85

BOB_PATH=tests/static_link/.bob
rm -rf $BOB_PATH

LIB1=$BOB_PATH/prefix/lib/lib1.a
LIB2=$BOB_PATH/prefix/lib/lib2.a
CMD=$BOB_PATH/prefix/bin/cmd

# Build everything and get initial times.

bob -C tests/static_link build
lib1_mtime=$(date -r $LIB1 +%s)
lib2_mtime=$(date -r $LIB2 +%s)
cmd_mtime=$(date -r $CMD +%s)

# Do nothing.
# We expect none of the files to be rebuilt.

sleep 1
bob -C tests/static_link build

[ $lib1_mtime -eq $(date -r $LIB1 +%s) ]
[ $lib2_mtime -eq $(date -r $LIB2 +%s) ]
[ $cmd_mtime -eq $(date -r $CMD +%s) ]

# Update lib2.
# cmd doesn't depend on this, so we only expect lib2 to be rebuilt.

sleep 1
touch tests/static_link/lib2.c
bob -C tests/static_link build

new_lib2_mtime=$(date -r $LIB2 +%s)

[ $lib1_mtime -eq $(date -r $LIB1 +%s) ]
[ $lib2_mtime -lt $new_lib2_mtime ]
[ $cmd_mtime -eq $(date -r $CMD +%s) ]

lib2_mtime=$new_lib2_mtime

# Update lib1.
# cmd does depend on this, so we expect cmd to be rebuilt as well.

sleep 1
touch tests/static_link/lib1.c
bob -C tests/static_link build

new_lib1_mtime=$(date -r $LIB1 +%s)
new_cmd_mtime=$(date -r $CMD +%s)

[ $lib1_mtime -lt $new_lib1_mtime ]
[ $lib2_mtime -eq $(date -r $LIB2 +%s) ]
[ $cmd_mtime -lt $new_cmd_mtime ]
