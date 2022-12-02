#!/bin/bash
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e

if [ ! -f "$1" ]; then
    echo "usage: $0 <path to downloaded debuginfo.zip>"
    exit 1
fi

debuginfo_archive=$(readlink -f "$1")

cd "$(dirname $0)"

docker run -p 12345:12345 \
    -v "$debuginfo_archive":/debuginfo.zip \
    -it ghcr.io/kdab/kdesrc-build \
    bash -c "echo 'unpacking debuginfo...' && \
    unzip -p - debuginfo.zip | tar -xvj && \
    echo 'starting debuginfod in docker...' && \
    echo 'to use it on your host system, set:' && \
    echo '' && \
    echo '    export DEBUGINFOD_URLS=\"127.0.0.1:12345 https://debuginfod.centos.org/ $DEBUGINFOD_URLS\"' && \
    echo '' && \
    debuginfod -p 12345 -F /usr /hotspot-debuginfo*/"
