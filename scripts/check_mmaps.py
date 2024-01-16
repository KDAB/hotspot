#!/bin/env python3
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

import sys
import re

prog = re.compile(r"\[(0x[a-z0-9]+)((0x[a-z0-9]+)\)")

for line in sys.stdin:
    if not "PERF_RECORD_MMAP" in line:
        continue

    m = prog.search(line)
    if not m:
        print("no match: ", line.rstrip())
        continue

    start = int(m[1], 16)
    length = int(m[2], 16)
    end = start + length

    for arg in sys.argv[1:]:
        if start <= int(arg, 16) < end:
            print(arg, "matched in:", line)
