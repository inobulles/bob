#!/bin/sh
set -e

. tests/common.sh

# Test the internal 'bob dep-tree' command and dependency tree creation.

# TODO Extra tests: make sure we can't depend on ourselves (this is a circular dependency, right? Will this be caught by our circular dependency check?)
# We need a circular dependency check of course.
# Also a check for a dependency which don't exit.
# And a check that we're caching dependencies correctly (i.e. same version same cache, different versions different cache).
# Also also a check that we don't have the same dependency twice in our list.

# Test dependency tree creation.

cp tests/deps/build{.normal,}.fl
rm -rf tests/deps/.bob tests/deps/dep{1,2}/.bob
bob -C tests/deps dep-tree >/dev/null 2>/dev/null

DEPS_TREE_PATH=tests/deps/.bob/deps.tree

if [ ! -f $DEPS_TREE_PATH ]; then
	echo "Dependency tree not created." >&2
	exit 1
fi

DEP1=$(find ~/.cache/bob/deps -name "*dep1*")
DEP2=$(find ~/.cache/bob/deps -name "*dep2*")

echo "$DEP1" > $DEPS_TREE_PATH.expected
echo "\t$DEP2" >> $DEPS_TREE_PATH.expected
echo "$DEP2" >> $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected." >&2
	exit 1
fi
