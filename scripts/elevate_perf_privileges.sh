#!/bin/sh

# 
# elevate_perf_privileges.sh
# 
# Temporarily elevate the privileges required to record advanced perf profiles.
#
# This file is part of Hotspot, the Qt GUI for performance analysis.
# 
# Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

echo "querying current privileges..."

old_sysctl_state=$(sysctl kernel.kptr_restrict kernel.perf_event_paranoid | sed 's/ = /=/')
old_debug_permissions=$(stat -c "%a" /sys/kernel/debug)
old_tracing_permissions=$(stat -c "%a" /sys/kernel/debug/tracing)

printPrivileges() {
    sysctl kernel.kptr_restrict kernel.perf_event_paranoid
    stat -c "%n %a" /sys/kernel/debug /sys/kernel/debug/tracing
}

printPrivileges
echo

# restore old privileges when exiting this script
cleanup() {
  echo "restoring old privileges..."
  sysctl -wq $old_sysctl_state
  mount -o remount,mode=$old_debug_permissions /sys/kernel/debug
  mount -o remount,mode=$old_tracing_permissions /sys/kernel/debug/tracing
  printPrivileges
}
trap cleanup EXIT

# handle term and int, such that the exit handler gets called (but only once)
# if we'd add INT and TERM to the cleanup trap above, it would get called twice
quit() {
    echo "quitting..."
}
trap quit TERM INT 

echo "elevating privileges..."
sysctl -wq kernel.kptr_restrict=0 kernel.perf_event_paranoid=-1
mount -o remount,mode=755 /sys/kernel/debug
mount -o remount,mode=755 /sys/kernel/debug/tracing
printPrivileges

echo
echo -n "waiting..."
read something
echo
