#!/bin/sh
set -e

. tests/common.sh

DIR=tests/dummy

cleanup() {
	rm -f $DIR/.gitignore
	rm -rf $DIR/.git
}

cleanup
trap cleanup EXIT # Cleans up when the program exits.

echo "Testing: no .git: no warning regardless."

out=$(bob -C $DIR build 2>&1)
echo "$out" | grep -qv "gitignore"

echo "Testing: .git but no .gitignore: warn."

mkdir $DIR/.git
out=$(bob -C $DIR build 2>&1)
echo "$out" | grep -q "No .gitignore found"

echo "Testing: .gitignore without .bob: warn."

echo "something_else" > $DIR/.gitignore
out=$(bob -C $DIR build 2>&1)
echo "$out" | grep -q "is not in .gitignore"

echo "Testing: .gitignore with .bob: no warning."

echo ".bob" >> $DIR/.gitignore
out=$(bob -C $DIR build 2>&1)
echo "$out" | grep -qv "gitignore"
