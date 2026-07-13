#!/bin/sh
set -e

. tests/common.sh

# Test installing programs.

SUDO .bootstrap/bob install
bob build

[ -f /usr/local/bin/bob ]
