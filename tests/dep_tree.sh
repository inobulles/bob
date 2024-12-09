#!/bin/sh

. tests/common.sh

# Test the internal 'bob dep-tree' command and dependency tree creation.

# TODO Extra tests: make sure we can't depend on ourselves (this is a circular dependency, right? Will this be caught by our circular dependency check?)
# We need a circular dependency check of course.
# Also a check for a dependency which don't exit.
# And a check that we're caching dependencies correctly (i.e. same version same cache, different versions different cache).
# Also also a check that we don't have the same dependency twice in our list.

try() {
	out=$(bob -C tests/deps dep-tree 2>&1)

	if [ $? != 0 ]; then
		echo "$1: $out" >&2
		exit 1
	fi
}

# Test dependency tree creation.

cp tests/deps/build.normal.fl tests/deps/build.fl
rm -rf tests/deps/.bob tests/deps/dep1/.bob tests/deps/dep2/.bob
try "Failed to create dependency tree"

DEPS_TREE_PATH=tests/deps/.bob/deps.tree

if [ ! -f $DEPS_TREE_PATH ]; then
	echo "Dependency tree not created." >&2
	exit 1
fi

DEP1=$(find $BOB_DEPS_PATH -name "*dep1*")
DEP2=$(find $BOB_DEPS_PATH -name "*dep2*")

echo -e "$DEP1" > $DEPS_TREE_PATH.expected
echo -e "\t$DEP2" >> $DEPS_TREE_PATH.expected
echo -e "$DEP2" >> $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected." >&2
	exit 1
fi

# Test that changing nothing doesn't explode.
# TODO Should this also be a frugality test?

try "Failed to create dependency tree after nothing changed"

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected after nothing changed." >&2
	exit 1
fi

# Test changing dependencies.

cp tests/deps/build.changed.fl tests/deps/build.fl
try "Failed to create dependency tree after changing dependencies"

echo -e "$DEP1" > $DEPS_TREE_PATH.expected
echo -e "\t$DEP2" >> $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected after changing dependencies." >&2
	exit 1
fi

# Test automatically skipping duplicate dependencies.

cp tests/deps/build.duplicate.fl tests/deps/build.fl
try "Failed to create dependency tree with duplicates in the dependency vector"

echo -e "$DEP2" > $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected with duplicates in the dependency vector." >&2
	exit 1
fi

# Test that circular dependencies fail as they should.

# cp tests/deps/build.circular.fl tests/deps/build.fl
# out=$(generic_timeout 2 bob -C tests/deps dep-tree 2>&1)
#
# if [ $? = 142 ]; then
# 	echo "Timed out when attempting to create a circular dependency tree (should just fail): $out" >&2
# 	exit 1
# fi
#
# if [ $? = 0 ]; then
# 	echo "Didn't fail when attempting to create a circular dependency tree: $out" >&2
# 	exit 1
# fi
