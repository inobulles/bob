#!/bin/sh
set -xe

URL=https://github.com/inobulles/flamingo

if [ $# -gt 1 ]; then
	echo "Usage: scripts/update-flamingo.sh [flamingo URL]"
	exit 1
fi

if [ $# -eq 1 ]; then
	URL=$1
fi

# Update flamingo.

rm -rf src/flamingo 2>/dev/null || true
git clone $URL --depth 1 --branch v0.1.5

mv flamingo/flamingo src/flamingo
rm -rf flamingo
