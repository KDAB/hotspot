#!/bin/bash

distro=$1
if [ -z "$distro" ]; then
    echo "missing distro arg"
    exit 1
fi

docker run -it hotspot-$distro bash
