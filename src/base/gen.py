# SPDX-License-Identifier: MIT
# Copyright (c) 2023 Aymeric Wibo

with open("base.wren") as f:
	base = f.read()

escaped = "".join("\\x" + hex(ord(c))[2:].zfill(2) for c in base)

src = f"""#pragma once
static char const* const base_src = "{escaped}";
"""

with open("base.h", "w") as f:
	f.write(src)
