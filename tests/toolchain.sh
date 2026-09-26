#!/bin/sh

. tests/common.sh

rm -rf tests/toolchain/.bob tests/toolchain/sentinel

bob -C tests/toolchain build 2>&1

if [ ! -f tests/toolchain/sentinel ]; then
	echo "Toolchain test failed: sentinel file not created (fake_cc.sh didn't run)" >&2
	exit 1
fi

rm -f tests/toolchain/sentinel
