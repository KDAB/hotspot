#!/bin/sh
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e

cd "$(dirname $0)"

mkdir -p ../output/build-appimage

docker run -it --rm \
    -v $PWD/../../:/github/workspace/ \
    ghcr.io/kdab/kdesrc-build:latest \
    /github/workspace/scripts/appimage/build_appimage.sh /github/workspace /github/workspace/scripts/output/build-appimage
