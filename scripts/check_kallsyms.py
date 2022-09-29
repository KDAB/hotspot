#!/bin/env python3
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

import sys
import re

prog = re.compile(r"([a-z0-9]+) (\S+) (\S+)(?:\s+(\S+))?")

prevLine = None
prevStart = None
nextLine = None
nextStart = None
addr = int(sys.argv[1], 16)

print("looking for", sys.argv[1])

for line in sys.stdin:
    m = prog.search(line)
    if not m:
        print("no match: ", line.rstrip())
        continue

    start = int(m[1], 16)
    if start > addr:
        nextLine = line
        nextStart = start
        break

    prevLine = line
    prevStart = start

print("best match sym:", prevLine.rstrip(), "diff is:", hex(addr - prevStart))
print("next sym is:", nextLine.rstrip(), "diff is:", hex(nextStart - addr))
