#!/bin/bash

cd $(dirname $0)/

SRCDIR=$PWD/d_demangler-version-0.0.2

OUTDIR=$PWD/artifacts

cd $SRCDIR

source ~/dlang/dmd-2.097.2/activate

make

mkdir $OUTDIR

cp /root/dlang/dmd-2.097.2/linux/lib64/libphobos2.so.0.97 .
tar -czvf d-demangle.tar.gz libd_demangle.so libphobos2.so.0.97

cp d-demangle.tar.gz $OUTDIR
