#!/bin/bash
#
# This file is part of Hotspot, the Qt GUI for performance analysis.
#
# Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
