#!/bin/sh

. tests/common.sh

# Regression test for the following PR:
# https://github.com/inobulles/bob/pull/70

# Make sure build.fl's Cc flags are correct.

if [ $(uname) = Linux ]; then # XXX Linux is annoying.
	sed -i 's/-std=c11/-std=c99/g' tests/frugality/build.fl
else
	sed -i '' 's/-std=c11/-std=c99/g' tests/frugality/build.fl
fi

# Clean up.

BOB_PATH=tests/frugality/.bob
rm -rf $BOB_PATH

# Functions for getting the mtimes of all cookies and preinstalled files.

build() {
	out=$(bob -C tests/frugality run 2>&1)

	if [ $? != 0 ]; then
		echo "Build failed: $out" >&2
		exit 1
	fi
}

get_mtimes() {
	export src1_${1}mtime=$(date -r $BOB_PATH/bob/src1.c.cookie.6531c664ecf.o +%s)
	export src2_${1}mtime=$(date -r $BOB_PATH/bob/src2.c.cookie.6531c665310.o +%s)
	export linked_${1}mtime=$(date -r $BOB_PATH/bob/linker.link.cookie.33faaddbf5af8a68.l +%s)

	export obj1_${1}mtime=$(date -r $BOB_PATH/prefix/obj1.o +%s)
	export obj2_${1}mtime=$(date -r $BOB_PATH/prefix/obj2.o +%s)
	export bin_${1}mtime=$(date -r $BOB_PATH/prefix/bin +%s)
}

update() {
	# We're running here instead of just building (even if there's nothing to run).
	# This is because we want the preinstallation step to run.

	build
	get_mtimes ""

	# If the mtime of the installed file is less than that of the cookie, then it was not reinstalled, which is bad!.

	if [ $obj1_mtime -lt $src1_mtime ]; then
		echo "obj1.o's cookie was updated, but it was not preinstalled." >&2
		exit 1
	fi

	if [ $obj2_mtime -lt $src2_mtime ]; then
		echo "obj2.o's cookie was updated, but it was not preinstalled." >&2
		exit 1
	fi

	if [ $bin_mtime -lt $linked_mtime ]; then
		echo "bin's cookie was updated, but it was not preinstalled." >&2
		exit 1
	fi

	# If the cookie was not updated, then it shouldn't have been preinstalled.

	export src1_updated=
	export src2_updated=
	export linked_updated=

	if [ $src1_mtime = $src1_prev_mtime ]; then
		if [ $obj1_mtime -gt $obj1_prev_mtime ]; then
			echo "obj1.o's cookie was not updated, but it was preinstalled." >&2
			exit 1
		fi
	elif [ $src1_mtime -gt $src1_prev_mtime ]; then
		src1_updated=true
	else
		echo "This is impossible." >&2
		exit 1
	fi

	if [ $src2_mtime = $src2_prev_mtime ]; then
		if [ $obj2_mtime -gt $obj2_prev_mtime ]; then
			echo "obj2.o's cookie was not updated, but it was preinstalled." >&2
			exit 1
		fi
	elif [ $src2_mtime -gt $src2_prev_mtime ]; then
		src2_updated=true
	else
		echo "This is impossible." >&2
		exit 1
	fi

	if [ $linked_mtime = $linked_prev_mtime ]; then
		if [ $bin_mtime -gt $bin_prev_mtime ]; then
			echo "bin's cookie was not updated, but it was preinstalled." >&2
			exit 1
		fi
	elif [ $linked_mtime -gt $linked_prev_mtime ]; then
		linked_updated=true
	else
		echo "This is impossible." >&2
		exit 1
	fi
}

build # Run this once to populate .bob already.

echo "Test 1: changing 'src2.c'."

get_mtimes "prev_"
sleep 1
touch tests/frugality/src2.c
update

if [ "$src1_updated" = true ] || [ -z $src2_updated ] || [ -z $linked_updated ]; then
	echo "Changing 'src2.c' failed." >&2
	exit 1
fi

echo "Test 2: changing both 'src1.c' and 'src2.c'."

get_mtimes "prev_"
sleep 1
touch tests/frugality/src1.c tests/frugality/src2.c
update

if [ -z $src1_updated ] || [ -z $src2_updated ] || [ -z $linked_updated ]; then
	echo "Changing 'src1.c' and 'src2.c' failed." >&2
	exit 1
fi

echo "Test 3: changing nothing."

get_mtimes "prev_"
sleep 1
update

if [ $src1_updated ] || [ $src2_updated ] || [ $linked_updated ]; then
	echo "Changing nothing failed." >&2
	exit 1
fi

echo "Test 4: changing 'src2.h'."

get_mtimes "prev_"
sleep 1
touch tests/frugality/src2.h
update

if [ $src1_updated ] || [ -z $src2_updated ] || [ -z $linked_updated ]; then
	echo "Changing 'src2.h' failed." >&2
	exit 1
fi

echo "Test 5: changing compilation flags for all source files."

get_mtimes "prev_"
sleep 1

if [ $(uname) = Linux ]; then # XXX Linux is annoying.
	sed -i 's/-std=c99/-std=c11/g' tests/frugality/build.fl
else
	sed -i '' 's/-std=c99/-std=c11/g' tests/frugality/build.fl
fi

update

if [ -z $src1_updated ] || [ -z $src2_updated ] || [ -z $linked_updated ]; then
	echo "Changing compilation flags failed." >&2
	exit 1
fi
