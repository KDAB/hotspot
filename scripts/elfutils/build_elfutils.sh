#!/bin/bash

cd $(dirname $0)/

OUTDIR=$PWD

PREFIX=/opt

SRCDIR=$PWD/src
BUILDDIR=$PWD/build

if [ ! -z "$1" ]; then
    PREFIX="$1"
fi

if [ ! -z "$2" ]; then
    OUTDIR=$(readlink -f $2)
fi

if [ ! -z "$3" ]; then
    SRCDIR=$(readlink -f $3)
fi

if [ ! -d "$BUILDDIR" ]; then
    mkdir "$BUILDDIR"
fi

cd "$BUILDDIR"
$SRCDIR/configure --prefix "$PREFIX"
make -j$(nproc)
make DESTDIR=$PWD/package install
tar cjvf "$OUTDIR/elfutils.build.tar.bz2" -C $PWD/package "${PREFIX:1}"
