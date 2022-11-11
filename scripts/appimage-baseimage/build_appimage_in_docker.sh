#!/bin/sh
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
mkdir -p ../output
docker run -it --privileged --device /dev/fuse --cap-add SYS_ADMIN \
    -v $PWD/../output:/output ghcr.io/kdab/kdesrc-build:latest \
    /bin/bash -c "git clone --recurse-submodules -j4 https://github.com/kdab/hotspot && cd hotspot && /build_appimage.sh"
