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

. /opt/rh/gcc-toolset-13/enable

mkdir -p "$buildir" && cd "$buildir"
# KGraphViewer triggers strange crashes in the AppImage, disable it
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH=/opt/rh/gcc-toolset-13/root/ \
    -DCMAKE_DISABLE_FIND_PACKAGE_KGraphViewerPart=ON \
    -DAPPIMAGE_BUILD=ON -DCMAKE_INSTALL_PREFIX=/usr "$srcdir"

make -j$(nproc)
rm -Rf appdir
DESTDIR=appdir make install

tar -cjvf "/github/workspace/hotspot-debuginfo-$gitversion-x86_64.tar.bz2" \
    --transform="s#appdir/#hotspot-debuginfo-$gitversion/#" \
    appdir/usr/bin/hotspot appdir/usr/lib64/libexec/hotspot-perfparser

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

# tell the linuxdeploy qt plugin to include these platform plugins
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so;libqwayland-generic.so"

mkdir -p appdir/usr/plugins/wayland-shell-integration/
cp /usr/plugins/wayland-shell-integration/libxdg-shell.so appdir/usr/plugins/wayland-shell-integration/

linuxdeploy --appdir appdir --plugin qt \
    -e "./appdir/usr/lib64/libexec/hotspot-perfparser" \
    -e "./appdir/usr/bin/hotspot" \
    -l "/usr/lib64/libz.so.1" \
    -l /usr/lib64/libharfbuzz.so.0 \
    -l /usr/lib64/libfreetype.so.6 \
    -l /usr/lib64/libfontconfig.so.1 \
    -l /usr/lib/librustc_demangle.so \
    -l /usr/lib/libd_demangle.so \
    -l /usr/lib64/libwayland-egl.so \
    -i "$srcdir/src/images/icons/128-apps-hotspot.png" --icon-filename=hotspot \
    -d "./appdir/usr/share/applications/com.kdab.hotspot.desktop" \
    --output appimage

mv Hotspot*x86_64.AppImage "/github/workspace/hotspot-$gitversion-x86_64.AppImage"
