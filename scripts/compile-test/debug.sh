#!/bin/bash

distro=$1
if [ -z "$distro" ]; then
    echo "missing distro arg"
    exit 1
fi

podman run -it hotspot-$distro bash
