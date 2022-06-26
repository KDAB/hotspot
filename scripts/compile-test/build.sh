#!/bin/bash

set -e

cd $(dirname $0)/../../

toLower()
{
    echo $1 | tr '[:upper:]' '[:lower:]'
}

buildBase()
{
    distro=$1
    tag=$(toLower $distro)

    docker build -t hotspot-$tag-base \
        -f scripts/compile-test/Base$distro .
}

buildDependencies()
{
    distro=$1
    tag=$(toLower $distro)

    buildBase $distro
    docker build -t hotspot-$tag-dependencies \
        --build-arg BASEIMAGE=hotspot-$tag-base \
        -f scripts/compile-test/BuildDependencies .
}

buildHotspotWithPresets()
{
    distro=$1
    tag=$(toLower $distro)

    buildDependencies $distro
    docker build -t hotspot-$tag \
        --build-arg BASEIMAGE=hotspot-$tag-dependencies \
        -f scripts/compile-test/BuildHotspotWithPresets .
}

buildHotspotWithoutPresets()
{
    distro=$1
    tag=$(toLower $distro)

    buildDependencies $distro
    docker build -t hotspot-$tag \
        --build-arg BASEIMAGE=hotspot-$tag-dependencies \
        -f scripts/compile-test/BuildHotspotWithoutPresets .
}

buildHotspotWithoutPresets Ubuntu20.04
buildHotspotWithPresets Ubuntu22.04
buildHotspotWithPresets Archlinux
buildHotspotWithPresets OpenSuseTumbleweed
buildHotspotWithoutPresets Fedora34
