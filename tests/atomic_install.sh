#!/bin/sh

. tests/common.sh

# Make sure installing over an existing file replaces it rather than rewriting it in place.

SRC=tests/atomic_install/file
PREFIX=$(pwd)/$TEST_OUT/atomic_install_prefix
INSTALLED=$PREFIX/share/file

rm -rf tests/atomic_install/.bob $PREFIX
mkdir -p $PREFIX

install() {
	out=$(bob -C tests/atomic_install -p $PREFIX install 2>&1)

	if [ $? != 0 ]; then
		echo "Install failed: $out" >&2
		exit 1
	fi
}

echo old > $SRC
install

if [ "$(cat $INSTALLED)" != old ]; then
	echo "File wasn't installed." >&2
	exit 1
fi

# Keep the installed file open across the reinstall.

prev_inode=$(ls -i $INSTALLED | awk '{print $1}')
exec 3< $INSTALLED

sleep 1 # Make sure the source file is newer than the installed one.
echo new > $SRC
install

if [ "$(cat $INSTALLED)" != new ]; then
	echo "File wasn't reinstalled." >&2
	exit 1
fi

if [ "$(ls -i $INSTALLED | awk '{print $1}')" = $prev_inode ]; then
	echo "File was rewritten in place rather than replaced." >&2
	exit 1
fi

if [ "$(cat <&3)" != old ]; then
	echo "Contents of the previously installed file changed from under it." >&2
	exit 1
fi

exec 3<&-

if [ -n "$(ls -A $PREFIX/share | grep -v '^file$')" ]; then
	echo "Temporary files were left behind: $(ls -A $PREFIX/share)" >&2
	exit 1
fi

rm $SRC
