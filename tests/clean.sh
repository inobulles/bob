#!/bin/sh
set -e

. tests/common.sh

# Test cleaning.

rm -rf .bob
.bootstrap/bob build

if [ ! -d .bob ]; then
	 echo "Build did not create output directory."
	 exit 1
fi

.bootstrap/bob clean < /dev/null # XXX Pipe in /dev/null to pass prompt.

if [ -d .bob ]; then
	 echo "Clean did not remove output directory."
	 exit 1
fi
