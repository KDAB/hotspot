#!/bin/sh
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -e

cd $(dirname $0)
docker build --ulimit nofile=1024:262144 --rm -t ghcr.io/kdab/kdesrc-build --target kdesrc-build .
docker build --ulimit nofile=1024:262144 --rm -t ghcr.io/kdab/kdesrc-build-debuginfo --target debuginfo .
