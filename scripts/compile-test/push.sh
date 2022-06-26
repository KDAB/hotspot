#!/bin/sh

set -e

ghcr_user=$(pass show ghcr.io | grep 'login: ' | cut -d' ' -f 2)
pass show ghcr.io | head -n1 | docker login ghcr.io -u $ghcr_user --password-stdin

logout() {
    docker logout ghcr.io
}
trap logout EXIT

pushDependencies()
{
    tag=$1

    docker tag hotspot-$tag-dependencies:latest ghcr.io/kdab/hotspot-$tag-dependencies:latest
    docker push ghcr.io/kdab/hotspot-$tag-dependencies:latest
}

pushDependencies ubuntu20.04
pushDependencies ubuntu22.04
pushDependencies archlinux
pushDependencies opensusetumbleweed
pushDependencies fedora34
