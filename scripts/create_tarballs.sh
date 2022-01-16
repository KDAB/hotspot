#!/bin/bash
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

version="$1"
output_dir="$2"

set -e

if [ -z "$version" -o -z "$output_dir" ]; then
  echo "create_tarballs.sh VERSION OUTPUT_DIR"
  echo
  echo "e.g.: create_tarballs.sh v1.1.0 /tmp"
  exit 1
fi

set -x

cd $output_dir

git clone --branch $version --recurse-submodules \
    git@github.com:KDAB/hotspot.git \
    "hotspot-$version"

tar --exclude="*/.git/*" --exclude="*/.git" -cvzf "hotspot-$version.tar.gz" "hotspot-$version"
tar --exclude="*/.git/*" --exclude="*/.git" -cvzf "hotspot-perfparser-$version.tar.gz" "hotspot-$version/3rdparty/perfparser"
zip -r --exclude="*/.git/*" --exclude="*/.git" "hotspot-$version.zip" "hotspot-$version"
zip -r --exclude="*/.git/*" --exclude="*/.git" "hotspot-perfparser-$version.zip" "hotspot-$version/3rdparty/perfparser"

md5sum "hotspot-$version.tar.gz" "hotspot-$version.zip" "hotspot-perfparser-$version.tar.gz" "hotspot-perfparser-$version.zip"
sha1sum "hotspot-$version.tar.gz" "hotspot-$version.zip" "hotspot-perfparser-$version.tar.gz" "hotspot-perfparser-$version.zip"
