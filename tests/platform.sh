#!/bin/sh

. tests/common.sh

# Tests for the Platform class.

# Test Platform.os().
# Should be the exact same output as uname(1).

out=$(bob -C tests/platform build 2>&1)

if [ "$out" != "$(uname)" ]; then
	echo "Platform.os() doesn't match uname(1) ('$out' vs '$(uname)')." >&2
	exit 1
fi
