#!/bin/sh
set -e

if [ -z "$CC" ]; then
	CC=cc
fi

mkdir -p sh-bin # 'bin' reserved for when compiling bob with bob

# compile all objects
# TODO Should we have to -Isrc/flamingo? Can it be made to just use quote includes?

cc_flags="
	-std=c11 -g -O0 -Isrc
	-isystem=/usr/local/include -L/usr/local/lib
	-Wall -Wextra -Werror -Wno-unused-parameter
	-Wno-unused-command-line-argument
	-lm
"

srcs=$(find src -name "*.c" -o -path src/flamingo -prune -type f)
srcs="$srcs src/flamingo/flamingo.c"

for src in $srcs; do
	mkdir -p sh-bin/$(dirname $src)

	obj=sh-bin/$src.o
	$CC $cc_flags -c $src -o $obj &
done

wait

# link everything together

objs=$(find sh-bin -name "*.o" -type f)
$CC $objs $cc_flags -o sh-bin/bob
