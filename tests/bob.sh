#!/bin/sh
set -e

# Test building Bob itself.

bob run -o $TEST_OUT/test-bob build
bob run -o $TEST_OUT/test-bob run -o $TEST_OUT/test-bob2 build
