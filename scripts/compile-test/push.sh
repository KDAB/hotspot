#!/bin/sh

set -e

ghcr_user=$(pass show ghcr.io | grep 'login: ' | cut -d' ' -f 2)
pass show ghcr.io | head -n1 | podman login ghcr.io -u $ghcr_user --password-stdin

logout() {
    podman logout ghcr.io
}
trap logout EXIT

pushDependencies()
{
    tag=$1

    # NOTE: qt6 suffix remains for compatibility reasons
    podman tag hotspot-$tag-dependencies:latest ghcr.io/kdab/hotspot/${tag}qt6-dependencies:latest
    podman push ghcr.io/kdab/hotspot/${tag}qt6-dependencies:latest
}

pushDependencies ubuntu25.04
pushDependencies archlinux
pushDependencies archlinuxwithoutoptional
pushDependencies opensusetumbleweed
pushDependencies fedora42
pushDependencies neon
