#!/bin/sh

. tests/common.sh

# Tests for the Platform class.

# Test Platform.os().
# Should be the exact same output as uname(1).

rm -rf tests/platform_os/.bob
out=$(bob -C tests/platform_os build 2>&1)

if [ "$out" != "$(uname)" ]; then
	echo "Platform.os() doesn't match uname(1) ('$out' vs '$(uname)')." >&2
	exit 1
fi

# Test Platform.getenv().
# Should be the exact same output as $PATH.

rm -rf tests/platform_getenv/.bob
out=$(bob -C tests/platform_getenv build 2>&1)

if [ "$out" != "$PATH" ]; then
	echo "Platform.getenv(\"PATH\") doesn't match \$PATH ('$out' vs '$PATH')." >&2
	exit 1
fi
