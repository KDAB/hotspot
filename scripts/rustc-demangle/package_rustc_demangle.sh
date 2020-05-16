#!/bin/bash

cd $(dirname $0)

artifacts="/tmp/package-rustc_demangle-artifacts"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts"
fi

sudo docker build -t package_rustc_demangle . || exit 1
sudo docker run -v "$artifacts":/artifacts -it package_rustc_demangle
mv -v "$artifacts"/rustc_demangle.build.tar.bz2 .
