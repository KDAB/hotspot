#!/bin/sh

cd $(dirname $0)/../

if [ ! -d hotspot-appimage-artifacts ]; then
    mkdir /tmp/hotspot-appimage-artifacts
fi

sudo docker build -t hotspot_appimage -f scripts/Dockerfile . || exit 1
sudo docker run -v /tmp/hotspot-appimage-artifacts:/artifacts -it hotspot_appimage
mv /tmp/hotspot-appimage-artifacts/hotspot-x86_64.AppImage hotspot-$(git describe)-x86_64.AppImage
ls -latr hotspot-*.AppImage | tail -n 1
