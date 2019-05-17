#!/bin/bash

cd $(dirname $0)

if [ ! -d /tmp/package-elfutils-artifacts ]; then
    mkdir /tmp/package-elfutils-artifacts
fi

sudo docker build -t package_elfutils . || exit 1
sudo docker run -v /tmp/package-elfutils-artifacts:/artifacts -it package_elfutils
mv -v /tmp/package-elfutils-artifacts/elfutils.build.tar.bz2 .
