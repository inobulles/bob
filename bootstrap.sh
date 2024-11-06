#!/bin/sh
set -e

if [ -z "$CC" ]; then
	CC=cc
fi

mkdir -p .bootstrap

# Compile all objects.

cc_flags="
	-std=c11 -g -O0 -Isrc
	-isystem=/usr/local/include -L/usr/local/lib
	-Isrc/flamingo/runtime
	-Wall -Wextra -Werror -Wno-unused-parameter
	-Wno-unused-command-line-argument
	-lm
	-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
"

srcs=$(find src -name "*.c" -o -path src/flamingo -prune -type f)
srcs="$srcs src/flamingo/flamingo.c"

for src in $srcs; do
	mkdir -p .bootstrap/$(dirname $src)

	obj=.bootstrap/$src.o
	$CC $cc_flags -c $src -o $obj &
done

wait

# Link everything together.

objs=$(find .bootstrap -name "*.o" -type f)
$CC $objs $cc_flags -o .bootstrap/bob