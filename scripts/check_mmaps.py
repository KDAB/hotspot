#!/bin/env python3
#
# check_mmaps.py
#
# This file is part of Hotspot, the Qt GUI for performance analysis.
#
# Copyright (C) 2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
# Author: Milian Wolff <milian.wolff@kdab.com>
#
# Licensees holding valid commercial KDAB Hotspot licenses may use this file in
# accordance with Hotspot Commercial License Agreement provided with the Software.
#
# Contact info@kdab.com if any conditions of this licensing are not clear to you.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import sys
import re

prog = re.compile("\[(0x[a-z0-9]+)\((0x[a-z0-9]+)\)")

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
