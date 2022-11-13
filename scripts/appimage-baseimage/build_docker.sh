#!/bin/sh
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -e

cd $(dirname $0)
docker build --rm -t ghcr.io/kdab/kdesrc-build .

echo "uploading to image to ghcr.io"

ghcr_user=$(pass show ghcr.io | grep 'login: ' | cut -d' ' -f 2)
pass show ghcr.io | head -n1 | docker login ghcr.io -u $ghcr_user --password-stdin

logout() {
    docker logout ghcr.io
}
trap logout EXIT

docker push ghcr.io/kdab/kdesrc-build:latest
