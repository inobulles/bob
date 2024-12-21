#!/bin/sh

. tests/common.sh

# Test Bob dependencies.
# TODO Check that we're caching dependencies correctly (i.e. same version same cache, different versions different cache).

# Test of regular usage of dependencies.

cp tests/deps/build.normal.fl tests/deps/build.fl
rm -rf tests/deps/.bob tests/deps/dep1/.bob tests/deps/dep2/.bob

out=$(bob -C tests/deps run dep2 2>&1)

if [ $? != 0 ]; then
	echo "Failed to run binary pre-installed by dependency: $out" >&2
	exit 1
fi
