#!/bin/sh

export ASAN_OPTIONS=detect_leaks=0 # XXX For now, let's not worry about leaks.
export TEST_OUT=.test-out
export BOB_DEPS_PATH=$(pwd)/.deps
export BOB_TARGET=bob-testing-target

# Find doas or sudo.

if [ $(id -u) = 0 ]; then
	alias SUDO=""
elif [ $(uname) = "Linux" ]; then
	# XXX Linux is annoying, 'which -s' doesn't exist.
	alias SUDO="sudo -E"
elif which -s doas; then
	alias SUDO="doas"
elif which -s sudo; then
	alias SUDO="sudo -E"
else
	echo "No sudo or doas found." >&2
	exit 1
fi

# Some multi-platform functions.

generic_timeout() {
	# Both options thankfully return 142 on timeout, because they both use SIGALRM.

	if [ $(uname) = Darwin ]; then
		# macOS doesn't have timeout(1), but it does have perl(1).
		perl -e 'alarm shift; exec @ARGV' "$@"
	else
		timeout $@
	fi
}
