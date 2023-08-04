#!/bin/sh

set -e

cd $(dirname $0)/..

rm -Rf scripts/fixits
mkdir -p scripts/fixits

run-clang-tidy -j $(nproc) -config-file .clang-tidy -export-fixes scripts/fixits/fixits.yaml -use-color -p $PWD/build $PWD/src

echo "splitting fixits"

./scripts/split-clang-tidy-fixits.py scripts/fixits/fixits.yaml

echo "fixits with auto replacements:"

grep -l Replacements:$ scripts/fixits/*/fixits.yaml | xargs dirname
