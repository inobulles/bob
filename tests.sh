#!/bin/sh

. tests/common.sh

echo "Bootstrapping Bob..."
sh bootstrap.sh

echo "Installing Bob..."
SUDO .bootstrap/bob install > /dev/null

# Actually run the tests.

all_passed=1

for test in $(ls -p tests | grep -v /); do
	if [ $test = "common.sh" ]; then
		continue
	fi

	if [ -d $test ]; then
		continue
	fi

	rm -rf .bob $TEST_OUT
	mkdir -p $TEST_OUT

	printf "Running test $test... "
	sh tests/$test > /dev/null

	if [ $? = 0 ]; then
		echo "PASSED"
	else
		echo "FAILED"
		all_passed=0
	fi
done

if [ $all_passed = 0 ]; then
	echo "TESTS FAILED!" >&2
	exit 1
else
	echo "ALL TESTS PASSED!"
fi
