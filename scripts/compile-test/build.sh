#!/bin/bash

cd ../../

docker build -f scripts/compile-test/Ubuntu .
docker build -f scripts/compile-test/Fedora .
docker build -f scripts/compile-test/Archlinux .
