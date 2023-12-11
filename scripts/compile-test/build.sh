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

    docker build --ulimit nofile=1024:262144 -t hotspot-$tag-base \
        -f scripts/compile-test/Base$distro $extraArgs .
}

buildDependencies()
{
    distro=$1
    kddw_version=$2

    tag=$(toLower $distro)

    buildBase $distro

    suffix=
    if [[ $distro == *Qt6 ]]; then
        suffix=Qt6
    fi

    docker build --ulimit nofile=1024:262144 -t hotspot-$tag-dependencies \
        --build-arg BASEIMAGE=hotspot-$tag-base --build-arg KDDOCKWIDGETS_VERSION="$kddw_version" \
        -f scripts/compile-test/BuildDependencies$suffix $extraArgs .
}

buildHotspotWithPresets()
{
    distro=$1
    kddw_version=$2
    tag=$(toLower $distro)

    suffix=
    if [[ $distro == *Qt6 ]]; then
        suffix=Qt6
    fi

    buildDependencies $distro $kddw_version
    docker build --ulimit nofile=1024:262144 -t hotspot-$tag \
        --build-arg BASEIMAGE=hotspot-$tag-dependencies \
        -f scripts/compile-test/BuildHotspotWithPresets$suffix $extraArgs .
}

buildHotspotWithoutPresets()
{
    distro=$1
    kddw_version=$2
    tag=$(toLower $distro)

    buildDependencies $distro $kddw_version
    docker build --ulimit nofile=1024:262144 -t hotspot-$tag \
        --build-arg BASEIMAGE=hotspot-$tag-dependencies \
        -f scripts/compile-test/BuildHotspotWithoutPresets $extraArgs .
}

export DOCKER_BUILDKIT=1
buildHotspotWithoutPresets Ubuntu20.04 1.6
buildHotspotWithPresets Ubuntu22.04 2.0
buildHotspotWithPresets Archlinux 2.0
buildHotspotWithPresets OpenSuseTumbleweed 2.0
buildHotspotWithoutPresets Fedora34 2.0
buildHotspotWithPresets NeonQt6 2.0
