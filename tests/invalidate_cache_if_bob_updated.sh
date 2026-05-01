#!/bin/sh
set -e

. tests/common.sh

VERSION_FILE=tests/fs/.bob/$BOB_TARGET/version

# Fresh build: version file should be created.

rm -rf tests/fs/.bob

bob -C tests/fs build > /dev/null 2>&1

[ -f $VERSION_FILE ]
[ -s $VERSION_FILE ]

# Same version: no "Bob was updated" warning.

out=$(bob -C tests/fs build 2>&1)

echo "$out" | grep -qv "Bob was updated"

# Stale version file: should warn and clear the cache.

echo "old-version" > $VERSION_FILE

out=$(bob -C tests/fs build 2>&1)

echo "$out" | grep -q "Bob was updated"

[ -f $VERSION_FILE ]
[ "$(cat $VERSION_FILE)" != "old-version" ]
