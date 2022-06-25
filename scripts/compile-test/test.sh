#!/bin/bash

set -e

cd $(dirname $0)/../../

# tst_perfdata is somehow completely broken on this version of ubuntu...
docker run hotspot-build-ubuntu bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc) -E tst_perfdata"
docker run hotspot-build-fedora bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc)"
docker run hotspot-build-archlinux bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc)"
docker run hotspot-build-opensuse bash -c "cd hotspot/build/ && ctest --output-on-failure -j \$(nproc)"
