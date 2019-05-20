#!/bin/sh

cd $(dirname $0)

artifacts=/tmp/hotspot-appimage-artifacts
elfutils_package="$PWD/elfutils/elfutils.build.tar.bz2"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts" || exit 1
fi

if [ ! -e "$elfutils_package" ]; then
    echo "missing \"$elfutils_package\" - did you run package_elfutil.sh already?"
    exit 1
fi

cd ..

sudo docker build -t hotspot_appimage -f scripts/Dockerfile . || exit 1
sudo docker run -v /tmp/hotspot-appimage-artifacts:/artifacts -it hotspot_appimage
mv /tmp/hotspot-appimage-artifacts/hotspot-x86_64.AppImage hotspot-$(git describe)-x86_64.AppImage
ls -latr hotspot-*.AppImage | tail -n 1
