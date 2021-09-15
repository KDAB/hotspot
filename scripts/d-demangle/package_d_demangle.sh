#!/bin/bash

cd $(dirname $0)

artifacts="/tmp/package-d_demangle-artifacts"

if [ ! -d "$artifacts" ]; then
    mkdir "$artifacts"
fi

sudo docker build -t package_d_demangle . || exit 1
sudo docker run -v "$artifacts":/opt/artifacts/ -it package_d_demangle
mv -v "$artifacts"/* .
