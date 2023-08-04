#!/bin/bash

set -e

cd $(dirname $0)/../../

testWithoutPreset()
{
    tag=$1
    shift 1
    docker run hotspot-$tag bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc) $@"
}

testWithPreset()
{
    tag=$1
    shift 1

    suffix=
    if [[ $tag == *qt6 ]]; then
        suffix=-qt6
    fi

    docker run hotspot-$tag bash -c "cd hotspot && ctest --preset dev-asan$suffix -j \$(nproc) $@"
}

testWithoutPreset ubuntu20.04 "-E tst_perfdata"
testWithoutPreset fedora34

testWithPreset ubuntu22.04
testWithPreset archlinux
testWithPreset opensusetumbleweed
testWithPreset neonqt6
