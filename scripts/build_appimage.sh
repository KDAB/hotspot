#!/bin/sh

cd $(dirname $0)/../

OUTDIR=$PWD

PREFIX=/opt

if [ ! -z "$1" ]; then
    PREFIX=$1
fi

if [ ! -z "$2" ]; then
    OUTDIR="$2"
fi

if [ -z "$(which linuxdeployqt)" ]; then
    echo "ERROR: cannot find linuxdeployqt in PATH"
    exit 1
fi

if [ -z "$(which appimagetool)" ]; then
    echo "ERROR: cannot find appimagetool in PATH"
    exit 1
fi

if [ ! -d build-appimage ]; then
    mkdir build-appimage
fi

cd build-appimage

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX -DAPPIMAGE_BUILD=ON
make -j$(nproc)
make DESTDIR=appdir install

# FIXME: Do in CMakeLists.txt
mkdir -p appdir/$PREFIX/share/applications/
cp ../hotspot.desktop appdir/$PREFIX/share/applications/

unset QTDIR
unset QT_PLUGIN_PATH
unset LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/qt510/lib/x86_64-linux-gnu # make sure this path is known so all Qt/KF5 libs are found
linuxdeployqt ./appdir/$PREFIX/share/applications/*.desktop -bundle-non-qt-libs
# workaround for https://github.com/KDAB/hotspot/issues/87
linuxdeployqt ./appdir/$PREFIX/lib/x86_64-linux-gnu/libexec/hotspot-perfparser -bundle-non-qt-libs -no-plugins

unset LD_LIBRARY_PATH
# also include the elfutils backend library ABI specific implementations
elfutils_libs_dir=$(find $PREFIX -path "*/elfutils/libebl_x86_64.so" -print -quit | xargs dirname)
cp -va "$elfutils_libs_dir/*.so" ./appdir/$PREFIX/lib/

# Share libraries to reduce image size
mv ./appdir/$PREFIX/lib/x86_64-linux-gnu/libexec/lib/* ./appdir/$PREFIX/lib/
rmdir ./appdir/$PREFIX/lib/x86_64-linux-gnu/libexec/lib
ln -sr ./appdir/$PREFIX/lib/ ./appdir/$PREFIX/lib/x86_64-linux-gnu/libexec/lib

# include breeze icons
if [ -d /opt/qt*/share/icons/breeze ]; then
    cp -a /opt/qt*/share/icons/breeze ./appdir/$PREFIX/share/icons/
fi

# Ensure we prefer the bundled libs also when calling dlopen, cf.: https://github.com/KDAB/hotspot/issues/89
rm ./appdir/AppRun
cat << WRAPPER_SCRIPT > ./appdir/AppRun
#!/bin/bash
f="\$(readlink -f "\${0}")"
d="\$(dirname "\$f")"
bin="\$d/$PREFIX/bin"
LD_LIBRARY_PATH="\$d/$PREFIX/lib":\$LD_LIBRARY_PATH "\$bin/hotspot" "\$@"
WRAPPER_SCRIPT
chmod +x ./appdir/AppRun

# Actually create the final image
appimagetool ./appdir $OUTDIR/hotspot-x86_64.AppImage
