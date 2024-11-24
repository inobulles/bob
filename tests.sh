#!/bin/sh

export ASAN_OPTIONS=detect_leaks=0 # XXX For now, let's not worry about leaks.
export TEST_OUT=.test-out

if which -s sudo; then
	SUDO=sudo
elif which -s doas; then
	SUDO=doas
else
	echo "No sudo or doas found."
	exit 1
fi

all_passed=1

for test in $(ls -p tests | grep -v /); do
	if [ -d $test ]; then
		continue
	fi

	rm -rf .bob $TEST_OUT
	mkdir -p $TEST_OUT

	echo -n "Running test $test... "
	sh tests/$test > /dev/null

	if [ $? = 0 ]; then
		echo "PASSED"
	else
		echo "FAILED"
		all_passed=0
	fi
done

if [ $all_passed = 0 ]; then
	echo "TESTS FAILED!"
	exit 1
else
	echo "ALL TESTS PASSED!"
fi
