#!/bin/bash

set -e

cd $(dirname $0)/../../

extraArgs=$@

toLower()
{
    echo $1 | tr '[:upper:]' '[:lower:]'
}

buildBase()
{
    distro=$1
    tag=$(toLower $distro)

    podman build --ulimit nofile=1024:262144 -t hotspot-$tag-base \
        -f scripts/compile-test/Base$distro $extraArgs .
}

buildDependencies()
{
    distro=$1
    kddw_version=$2

    tag=$(toLower $distro)

    buildBase $distro

    libdir="/usr/lib"
    if [[ $distro == Neon* || $distro == Ubuntu* ]]; then
        libdir="/usr/lib/x86_64-linux-gnu/"
    fi

    podman build --ulimit nofile=1024:262144 -t hotspot-$tag-dependencies \
        --build-arg BASEIMAGE=hotspot-$tag-base --build-arg KDDOCKWIDGETS_VERSION="$kddw_version" \
        --build-arg LIBDIR="$libdir" \
        -f scripts/compile-test/BuildDependencies $extraArgs .
}

buildHotspotWithPresets()
{
    distro=$1
    kddw_version=$2
    tag=$(toLower $distro)

    preset=$3
    if [ -z "$preset" ]; then
        preset="dev-asan"
    fi

    buildDependencies $distro $kddw_version
    podman build --ulimit nofile=1024:262144 -t hotspot-$tag-$preset \
        --build-arg BASEIMAGE=hotspot-$tag-dependencies \
        --build-arg PRESET=$preset \
        -f scripts/compile-test/BuildHotspotWithPresets $extraArgs .
}

buildHotspotWithPresets Ubuntu25.04 2.2
buildHotspotWithPresets Archlinux 2.3
buildHotspotWithPresets Archlinux 2.3 dev-clazy
buildHotspotWithPresets ArchlinuxWithoutOptional 2.3
buildHotspotWithPresets OpenSuseTumbleweed 2.3
buildHotspotWithPresets Fedora42 2.3
buildHotspotWithPresets Neon 2.3
