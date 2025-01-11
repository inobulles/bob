#!/bin/sh

. tests/common.sh

# Test the internal 'bob dep-tree' command and dependency tree creation.

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

printf "dep1:$DEP1\n" > $DEPS_TREE_PATH.expected
printf "\tdep2:$DEP2\n" >> $DEPS_TREE_PATH.expected
printf "dep2:$DEP2\n" >> $DEPS_TREE_PATH.expected

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

printf "dep1:$DEP1\n" > $DEPS_TREE_PATH.expected
printf "\tdep2:$DEP2\n" >> $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected after changing dependencies." >&2
	exit 1
fi

# Test automatically skipping duplicate dependencies.

cp tests/deps/build.duplicate.fl tests/deps/build.fl
try "Failed to create dependency tree with duplicates in the dependency vector"

printf "dep2:$DEP2\n" > $DEPS_TREE_PATH.expected

if ! diff $DEPS_TREE_PATH $DEPS_TREE_PATH.expected; then
	echo "Dependency tree differed to the one expected with duplicates in the dependency vector." >&2
	exit 1
fi

# Test that circular dependencies fail as they should.

cp tests/deps/build.circular.fl tests/deps/build.fl
out=$(generic_timeout 2 bob -C tests/deps dep-tree 2>&1)

if [ $? = 142 ]; then
	echo "Timed out when attempting to create a circular dependency tree: $out" >&2
	exit 1
fi

if [ $? != 0 ]; then
	echo "Failed when attempting to create a circular dependency tree: $out" >&2
	exit 1
fi

if ! echo $out | grep -q "<bob-dep-tree circular />"; then
	echo "Circular dependency not detected." >&2
	exit 1
fi

# Test that we can't depend on ourselves.

cp tests/deps/build.self.fl tests/deps/build.fl
out=$(generic_timeout 2 bob -C tests/deps dep-tree 2>&1)

if [ $? = 142 ]; then
	echo "Timed out when attempting to create a self dependency tree: $out" >&2
	exit 1
fi

if [ $? != 0 ]; then
	echo "Failed when attempting to create a self dependency tree: $out" >&2
	exit 1
fi

if ! echo $out | grep -q "<bob-dep-tree circular />"; then
	echo "Self dependency not detected." >&2
	exit 1
fi

# Test that we can't depend on dependencies which don't exist.

cp tests/deps/build.nonexistent.fl tests/deps/build.fl
out=$(bob -C tests/deps dep-tree 2>&1)

if [ $? = 0 ]; then
	echo "Succeeded when attempting to create a dependency tree with a dependency which doesn't exist: $out" >&2
	exit 1
elif ! echo $out | grep -q "Could not find local dependency at 'this-dep-doesnt-exist'"; then
	echo "Another issue occurred when attempting to create a dependency tree with a dependency which doesn't exist: $out" >&2
	exit 1
fi

# Test that we output an empty tree if there are no dependencies.
# Regression test for d1619dd "bsys: Output empty dependency tree if bsys doesn't have `dep_tree` function".

cp tests/deps/build.none.fl tests/deps/build.fl
out=$(bob -C tests/deps dep-tree 2>&1)

if [ $? != 0 ]; then
	echo "Failed when attempting to create a dependency tree with no dependencies: $out" >&2
	exit 1
elif ! echo $out | grep -q "<bob-dep-tree> </bob-dep-tree>"; then
	echo "Dependency tree not empty when attempting to create a dependency tree with no dependencies: $out" >&2
	exit 1
fi
