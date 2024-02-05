#!/bin/bash
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e

srcdir=$(readlink -f "$1")
buildir=$(readlink -f "$2")

if [ -z "$srcdir" ] || [ -z "$buildir" ]; then
    echo "usage: $0 <srcdir> <builddir>"
    exit 1
fi

gitversion=$(git -C "$srcdir" describe)

. /opt/rh/devtoolset-11/enable

mkdir -p "$buildir" && cd "$buildir"
# KGraphViewer triggers strange crashes in the AppImage, disable it
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH=/opt/rh/devtoolset-11/root/ \
    -DCMAKE_DISABLE_FIND_PACKAGE_KGraphViewerPart=ON \
    -DAPPIMAGE_BUILD=ON "-DCMAKE_INSTALL_PREFIX=/usr" "$srcdir"

make -j
DESTDIR=appdir make install

# FIXME: Do in CMakeLists.txt
mkdir -p "appdir/usr/share/applications/"
cp "$srcdir/com.kdab.hotspot.desktop" "appdir/usr/share/applications/"

# Ensure we prefer the bundled libs also when calling dlopen, cf.: https:/github.com/KDAB/hotspot/issues/89
cat << WRAPPER_SCRIPT > ./appdir/AppRun
#!/bin/bash
f="\$(readlink -f "\${0}")"
d="\$(dirname "\$f")"
bin="\$d/usr/bin"
unset QT_PLUGIN_PATH
LD_LIBRARY_PATH="\$d/usr/lib":\$LD_LIBRARY_PATH "\$bin/hotspot" "\$@"
WRAPPER_SCRIPT
chmod +x ./appdir/AppRun

# include breeze icons
mkdir -p "appdir/usr/share/icons/breeze"
cp -v "/usr/share/icons/breeze/breeze-icons.rcc" "appdir/usr/share/icons/breeze/"

# plugins

# FIXME: kgraphviewer crashes when loading a file when compiled with this method
# mkdir ./appdir/usr/plugins
# cp /usr/plugins/kgraphviewerpart.so ./appdir/usr/plugins
# TODO: further down also add:
# -e "./appdir/usr/plugins/kgraphviewerpart.so" \

linuxdeploy-x86_64.AppImage --appdir appdir --plugin qt \
    -e "./appdir/usr/lib64/libexec/hotspot-perfparser" \
    -e "./appdir/usr/bin/hotspot" \
    -l "/usr/lib64/libz.so.1" \
    -l /usr/lib64/libharfbuzz.so.0 \
    -l /usr/lib64/libfreetype.so.6 \
    -l /usr/lib64/libfontconfig.so.1 \
    -l /usr/lib/librustc_demangle.so \
    -l /usr/lib/libd_demangle.so \
    -i "$srcdir/src/images/icons/512-hotspot_app_icon.png" --icon-filename=hotspot \
    -d "./appdir/usr/share/applications/com.kdab.hotspot.desktop" \
    --output appimage

# copy qt libs and modify debug link since linuxdeploy modifies the lib
for lib in $(cd appdir/usr/lib && ls libQt*); do
    if [ -f /usr/lib/${lib}*.debug ]; then
        cp /usr/lib/${lib}*.debug appdir/usr/lib/${lib}.debug
		objcopy --add-gnu-debuglink="appdir/usr/lib/${lib}" appdir/usr/lib/${lib}.debug
    fi
done

# kde libs got stripped by linuxdeploy so we need to extrace the debug info from the source files
for lib in $(cd appdir/usr/lib && ls libKF*); do
	strip --only-keep-debug /usr/lib64/${lib}.* -o appdir/usr/lib/${lib}.debug
done

# copy debug infos for kddockwidgets, hotspot and perfparser
strip --only-keep-debug bin/hotspot -o appdir/usr/bin/hotspot.debug
strip --only-keep-debug bin/perfparser -o appdir/usr/lib64/libexec/hotspot-perfparser.debug
strip --only-keep-debug /usr/lib64/libkddockwidgets.so.2.0 -o appdir/usr/lib/libkddockwidgets.so.2.0.debug

# copy elfutils debuginfo
cp /usr/lib/debug/opt/rh/devtoolset-11/root/usr/lib64/lib{elf,dw}-*.debug appdir/usr/lib

# add debug files from libs
tar -cjvf "/output/hotspot-debuginfo-$gitversion-x86_64.tar.bz2" \
    --transform="s#appdir/#hotspot-debuginfo-$gitversion/#" \
    appdir/usr/bin/hotspot.debug appdir/usr/lib64/libexec/hotspot-perfparser.debug \
	appdir/usr/lib/lib*.debug

# delete debug info from final appimage
find appdir -type f -name "*.debug" -exec rm {} \;

find appdir/usr/lib -type f -name "*.so*" -exec strip -s {} \;

# package appdir with type 2 runtime so we don't depend on glibc and fuse2
appimagetool-x86_64.AppImage --runtime-file /opt/runtime-x86_64 appdir

mv Hotspot*x86_64.AppImage "/output/hotspot-$gitversion-x86_64.AppImage"
