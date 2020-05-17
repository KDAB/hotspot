#!/bin/sh

cd $(dirname $0)

artifacts=/tmp/hotspot-appimage-artifacts
elfutils_package="$PWD/elfutils/elfutils.build.tar.bz2"
rustc_demangle_package="$PWD/rustc-demangle/rustc_demangle.build.tar.bz2"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts" || exit 1
fi

if [ ! -e "$elfutils_package" ]; then
    echo "missing \"$elfutils_package\" - did you run package_elfutils.sh already?"
    exit 1
fi

if [ ! -e "$rustc_demangle_package" ]; then
    echo "missing \"$rustc_demangle_package\" - did you run package_rustc_demangle.sh already?"
    exit 1
fi

cd ..

sudo docker build -t hotspot_appimage -f scripts/Dockerfile . || exit 1
sudo docker run -v "$artifacts":/artifacts -it hotspot_appimage
mv "$artifacts"/hotspot-x86_64.AppImage hotspot-$(git describe)-x86_64.AppImage
ls -latr hotspot-*.AppImage | tail -n 1
