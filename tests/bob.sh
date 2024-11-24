#!/bin/sh
set -e

. tests/common.sh

# Test building Bob itself.

bob run -o $TEST_OUT/test-bob build
bob run -o $TEST_OUT/test-bob run -o $TEST_OUT/test-bob2 build
