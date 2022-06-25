#!/bin/bash

set -e

cd $(dirname $0)/../../

# tst_perfdata is somehow completely broken on this version of ubuntu...
docker run hotspot-build-ubuntu20.04 bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc) -E tst_perfdata"
docker run hotspot-build-fedora bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc)"
docker run hotspot-build-opensuse bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc)"
# more modern variants with presets
docker run hotspot-build-ubuntu22.04 bash -c "cd hotspot && ctest --preset dev-asan -j \$(nproc)"
docker run hotspot-build-archlinux bash -c "cd hotspot && ctest --preset dev-asan -j \$(nproc)"
