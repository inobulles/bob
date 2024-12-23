#!/bin/sh

. tests/common.sh

# Tests for the Fs class.

rm -rf tests/fs/.bob
out=$(bob -C tests/fs build 2>&1)

if [ $? != 0 ]; then
	echo "Tests for the Fs class failed: $out" >&2
	exit 1
fi
