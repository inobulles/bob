#!/bin/sh
set -e

. tests/common.sh

# Test that artifacts are output on 'bob build', not just 'bob install'.

rm -rf tests/artifact/.bob tests/artifact/soy-un-artifact

out=$(bob -C tests/artifact build 2>&1)

if [ $? != 0 ]; then
	echo "Artifact test failed: $out" >&2
	exit 1
fi

[ -f tests/artifact/soy-un-artifact ]
