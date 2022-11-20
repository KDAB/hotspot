#!/bin/bash
#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -e

. /opt/rh/devtoolset-11/enable

mkdir build && cd build
PREFIX=/usr
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH=/opt/rh/devtoolset-11/root/ \
    -DAPPIMAGE_BUILD=ON "-DCMAKE_INSTALL_PREFIX=$PREFIX" ..

make -j
DESTDIR=appdir make install

# FIXME: Do in CMakeLists.txt
mkdir -p "appdir/$PREFIX/share/applications/"
cp ../com.kdab.hotspot.desktop "appdir/$PREFIX/share/applications/"

# Ensure we prefer the bundled libs also when calling dlopen, cf.: https://github.com/KDAB/hotspot/issues/89
cat << WRAPPER_SCRIPT > ./appdir/AppRun
#!/bin/bash
f="\$(readlink -f "\${0}")"
d="\$(dirname "\$f")"
bin="\$d/$PREFIX/bin"
unset QT_PLUGIN_PATH
LD_LIBRARY_PATH="\$d/$PREFIX/lib":\$LD_LIBRARY_PATH "\$bin/hotspot" "\$@"
WRAPPER_SCRIPT
chmod +x ./appdir/AppRun

# include breeze icons
mkdir -p "appdir/$PREFIX/share/icons/breeze"
cp -v "/usr/share/icons/breeze/breeze-icons.rcc" "appdir/$PREFIX/share/icons/breeze/"

# plugins

# FIXME: kgraphviewer crashes when loading a file when compiled with this method
# mkdir ./appdir/$PREFIX/plugins
# cp /usr/plugins/kgraphviewerpart.so ./appdir/$PREFIX/plugins
# TODO: further down also add:
# -e "./appdir/$PREFIX/plugins/kgraphviewerpart.so" \

linuxdeploy-x86_64.AppImage --appdir appdir --plugin qt \
    -e "./appdir/$PREFIX/lib64/libexec/hotspot-perfparser" \
    -e "./appdir/$PREFIX/bin/hotspot" \
    -l "/usr/lib64/libz.so.1" \
    -l /usr/lib64/libharfbuzz.so.0 \
    -l /usr/lib64/libfreetype.so.6 \
    -l /usr/lib64/libfontconfig.so.1 \
    -l /usr/lib/librustc_demangle.so \
    -l /usr/lib/libd_demangle.so \
    -i ../src/images/icons/512-hotspot_app_icon.png --icon-filename=hotspot \
    -d "./appdir/$PREFIX/share/applications/com.kdab.hotspot.desktop" \
    --output appimage

mv Hotspot-*-x86_64.AppImage /output/hotspot-$(git describe)-x86_64.AppImage
