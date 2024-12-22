#!/bin/sh

. tests/common.sh

# Regression test for the following PR:
# https://github.com/inobulles/bob/pull/69
# Basically, there's this feature which chown's generated files to the owner of the project directory when running Bob as root.
# The issue though is that, before #69, even installed files would be chown'd to the owner of the project directory.
# The result was that on FreeBSD, the ldconfig service would refuse to cache the libraries in /usr/local/lib because they weren't owned by root, which is obviously very bad as nothing that linked against those would work.

# Make sure everything is clean.

INSTALL_PATH=/usr/local/share/bob_chown_test.fl
SUDO rm -rf tests/chown/.bob $INSTALL_PATH

# Create a user.

USER=bobtestuser

if [ $(uname) = "FreeBSD" ]; then
	SUDO pw adduser -n $USER
elif [ $(uname) = "Linux" ]; then
	SUDO useradd -m $USER
elif [ $(uname) = "Darwin" ]; then
	out=$(SUDO sysadminctl -addUser $USER 2>&1)

	if [ $? != 0 ]; then
		echo "Failed to create user: $out" >&2
		exit 1
	fi
else
	echo "Unsupported OS (don't know how to create a user)." >&2
	exit 1
fi

# Chown the project directory to that user.

SUDO chown -R bobtestuser tests/chown
SUDO bob -C tests/chown build

if [ ! -z "$(find tests/chown ! -user bobtestuser)" ]; then
	echo "Some files are not owned by bobtestuser in tests/chown." >&2
	exit 1
fi

SUDO bob -C tests/chown install

# XXX Annoyingly, 'stat -f %Su' doesn't work on Linux (you have to use 'stat -c %U').

owner=$(ls -ld /usr/local/share/bob_chown_test.fl | awk '{print $3}')

if [ $owner != "root" ]; then
	echo "Installed files are not owned by root." >&2
	exit 1
fi

# chown back to the original user, so things aren't annoying.

uid=$(id -u)
SUDO chown -R $uid tests/chown
