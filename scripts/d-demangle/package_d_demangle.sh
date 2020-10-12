#!/bin/bash

cd $(dirname $0)

artifacts="/tmp/package_ddemangle-artifacts"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts"
fi

sudo docker build -t package_d_demangle . || exit
sudo docker run -v "$artifacts":/artifacts -it package_d_demangle cp ddemangle.build.tar.bz /artifacts
mv -v "$artifacts"/ddemangle.build.tar.bz .
