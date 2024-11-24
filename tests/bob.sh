#!/bin/sh
set -e

# Test building Bob itself.

.bootstrap/bob run -o $TEST_OUT/test-bob build
.bootstrap/bob run -o $TEST_OUT/test-bob run -o $TEST_OUT/test-bob2 build
