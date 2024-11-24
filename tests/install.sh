#!/bin/sh
set -e

# Test installing programs.

if which -s sudo; then
	SUDO=sudo
elif which -s doas; then
	SUDO=doas
else
	echo "No sudo or doas found."
	exit 1
fi

$SUDO .bootstrap/bob install
bob build

[ -d /usr/local/share/bob/skeletons ]
[ -f /usr/local/bin/bob ]
