#!/bin/sh
set -xe

mkdir -p sh-bin # 'bin' reserved for when compiling bob with bob

# compile all objects

cc_flags="
	-std=c99 -g -O0 -Isrc/wren/include -Isrc
	-isystem=/usr/local/include -L/usr/local/lib
	-DWREN_OPT_META=0 -DWREN_OPT_RANDOM=0
	-Wno-unused-command-line-argument
	-lm
"

for src in $(find src -name "*.c" -type f); do
	mkdir -p sh-bin/$(dirname $src)

	obj=sh-bin/$src.o
	cc $cc_flags -c $src -o $obj &
done

wait

# link everything together

objs=$(find sh-bin -name "*.o" -type f)
cc $objs $cc_flags -o sh-bin/bob
