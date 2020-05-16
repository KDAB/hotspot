#!/bin/bash

cd $(dirname $0)/

OUTDIR=$PWD

PREFIX=/opt

SRCDIR=$PWD/src

if [ ! -z "$1" ]; then
    PREFIX="$1"
fi

if [ ! -z "$2" ]; then
    OUTDIR=$(readlink -f $2)
fi

if [ ! -z "$3" ]; then
    SRCDIR=$(readlink -f $3)
fi

cd "$SRCDIR"
cargo build -p rustc-demangle-capi --release

if [ ! -d "package/$PREFIX/lib" ]; then
    mkdir -p "package/$PREFIX/lib"
fi

if [ ! -d "package/$PREFIX/include" ]; then
    mkdir -p "package/$PREFIX/include"
fi

cp "$PWD/target/release/librustc_demangle.so" "$PWD/package/$PREFIX/lib"
cp "$PWD/crates/capi/include/rustc_demangle.h" "$PWD/package/$PREFIX/include"
tar cjvf "$OUTDIR/rustc_demangle.build.tar.bz2" -C "$PWD/package" "${PREFIX:1}"
