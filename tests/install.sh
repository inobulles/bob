#!/bin/sh
set -e

# Test installing programs.

$SUDO bob install
bob build

[ -d /usr/local/share/bob/skeletons ]
[ -f /usr/local/bin/bob ]
