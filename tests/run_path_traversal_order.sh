#!/bin/sh

# Regression test for the following PR:
# https://github.com/inobulles/bob/pull/71

out=$(bob -C tests/run_path_traversal_order run 2>/dev/null)

if [ $? == 0 ]; then
	echo "\$PATH was not set in order to find command; running should not have succeeded: $out" >&2
	exit 1
fi

bin1=$(realpath tests/run_path_traversal_order/bin1)
bin2=$(realpath tests/run_path_traversal_order/bin2)

ORIG_PATH=$PATH
export PATH="$bin1:$bin2:$ORIG_PATH"
out=$(bob -C tests/run_path_traversal_order run 2>/dev/null)

if ! [ "$out" = "bin1" ]; then
	echo "Ran binary from the wrong path or some other issue occurred: $out" >&2
	exit 1
fi

export PATH="$bin2:$bin1:$ORIG_PATH"
out=$(bob -C tests/run_path_traversal_order run 2>/dev/null)

if ! [ "$out" = "bin2" ]; then
	echo "Ran binary from the wrong path or some other issue occurred: $out" >&2
	exit 1
fi
