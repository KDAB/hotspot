#!/bin/bash

set -e

cd $(dirname $0)/../../

testWithPreset()
{
    tag=$1
    shift 1

    podman run hotspot-$tag-dev-asan bash -c "cd hotspot && ctest --preset dev-asan -j \$(nproc) $@"
}

testWithPreset ubuntu25.04
testWithPreset archlinux
testWithPreset opensusetumbleweed
testWithPreset fedora42
testWithPreset neon
