#!/bin/bash

set -e

cd $(dirname $0)/../../

docker build -t hotspot-build-ubuntu -f scripts/compile-test/Ubuntu .
docker build -t hotspot-build-fedora -f scripts/compile-test/Fedora .
docker build -t hotspot-build-archlinux -f scripts/compile-test/Archlinux .
docker build -t hotspot-build-opensuse -f scripts/compile-test/OpenSuse .
