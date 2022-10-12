#!/bin/sh
set -xe

# TODO actually it isn't that smart to link with umber if umber itself needs bob to compile
#      ideally, bob should rely on literally nothing else than what POSIX provides (except in the case of OS-specific stuff obviously)

mkdir -p bin

# compile all objects

cc_flags="
	-std=c99 -g -O2 -Isrc/wren/include
	-I/usr/local/include -L/usr/local/lib
	-DWREN_OPT_META=0 -DWREN_OPT_RANDOM=0
	-Wno-unused-command-line-argument
	-lm -lumber
"

for src in $(find src -name "*.c" -type f); do
	obj=bin/$(basename $src).o

	# to speed things up, skip compilation if source file is older than current object file

	if [ -f $obj ]; then
		src_age=$(date -r $src +%s)
		obj_age=$(date -r $obj +%s)

		if [ $src_age -lt $obj_age ]; then
			continue
		fi
	fi

	cc $cc_flags -c $src -o $obj &
done

wait

# link everything together

objs=$(find bin -name "*.o" -type f)
cc $objs $cc_flags -o bin/bob
