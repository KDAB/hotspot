#!/bin/bash

cd $(dirname $0)

artifacts="/tmp/package-elfutils-artifacts"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts"
fi

sudo docker build -t package_elfutils . || exit 1
sudo docker run -v "$artifacts":/artifacts -it package_elfutils
mv -v "$artifacts"/elfutils.build.tar.bz2 .
