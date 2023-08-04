#!/bin/sh

set -e

if [ ! -d "$1" ]; then
    echo "usage: run_clang_tidy.sh <build_dir>"
    exit 1
fi
build_dir=$(readlink -f "$1")

cd $(dirname $0)/..

rm -Rf scripts/fixits
mkdir -p scripts/fixits

run-clang-tidy -extra-arg="-Wno-gnu-zero-variadic-macro-arguments" -j $(nproc) -config-file .clang-tidy -export-fixes scripts/fixits/fixits.yaml -use-color -p "$build_dir" "$PWD/src"

if [ -s "scripts/fixits/fixits.yaml" ]; then
    echo "splitting fixits"

    ./scripts/split-clang-tidy-fixits.py scripts/fixits/fixits.yaml

    echo "fixits with auto replacements:"

    grep -l Replacements:$ scripts/fixits/*/fixits.yaml | xargs dirname
fi
