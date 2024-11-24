#!/bin/sh

. tests/common.sh

# Test the 'bob sh' command.
# Also kind of a regression test for the following PR:
# https://github.com/inobulles/bob/pull/77

if [ "$(bob sh echo test | tail -n1)" != "test" ]; then
	echo "Failed to run command in shell." >&2
	exit 1
fi

if [ "$(bob sh -- sh -c "echo \$PATH" | tail -n1 | cut -d':' -f1)" != ".bob/prefix/bin" ]; then
	echo "Temporary installation prefix not in \$PATH." >&2
	exit 1
fi

# XXX Can't really test 'bob sh' on it's own "properly" I don't think.

export SHELL=uname

if [ "$(bob sh | tail -n1)" != "$(uname)" ]; then
	echo "Failed to run command in shell." >&2
	exit 1
fi
