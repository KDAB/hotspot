#!/bin/sh
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

if [ "$(id -u)" != "0" ]; then
   echo "Error: This script must be run as root"
   exit 1
fi

if [ ! -z "$1" ]; then
    olduser=$(stat -c '%u' "$1")
    chown "$(whoami)" "$1"
    echo "rewriting to $1"
    # redirect output to file, to enable parsing of output even when
    # the graphical sudo helper like kdesudo isn't forwarding the text properly
    $0 2>&1 | tee -a "$1"
    chown "$olduser" "$1"
    exit
fi

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
echo "privileges elevated!"
read something 2> /dev/null
echo
