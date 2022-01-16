#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

# trusty
FROM ubuntu:18.04 as hotspot_appimage_intermediate

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y software-properties-common build-essential curl git wget \
        autotools-dev autoconf libtool liblzma-dev libz-dev gettext && \
    add-apt-repository ppa:beineri/opt-qt-5.15.2-bionic -y && \
    apt-get update && \
    apt-get install -y qt515base qt515svg qt515x11extras cmake libdwarf-dev mesa-common-dev \
        libboost-iostreams-dev libboost-program-options-dev libboost-system-dev libboost-filesystem-dev

WORKDIR /opt

# download and build zstd

RUN wget -nv -c "https://github.com/facebook/zstd/archive/v1.4.5.tar.gz" -O zstd.tar.gz && \
    tar xf zstd.tar.gz && cd zstd-1.4.5 && make -j$(nproc) && make install && cd ..

# download prebuild KF5 libraries and ECM

RUN wget -c "https://github.com/chigraph/precompiled-kf5-linux/releases/download/precompiled/kf5-gcc6-linux64-release.tar.xz" && \
    tar --strip-components=2 -xf kf5-gcc6-linux64-release.tar.xz -C /opt/qt515/

# download AppImage tools and extract them, to remove fuse dependency

RUN wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" -O /tmp/linuxdeployqt && \
    wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O /tmp/appimagetool && \
    chmod a+x /tmp/linuxdeployqt /tmp/appimagetool && \
    /tmp/linuxdeployqt --appimage-extract && mv squashfs-root linuxdeployqt && \
    /tmp/appimagetool --appimage-extract && mv squashfs-root appimagetool && \
    mkdir /opt/bin && \
    ln -s /opt/linuxdeployqt/AppRun /opt/bin/linuxdeployqt && \
    ln -s /opt/appimagetool/AppRun /opt/bin/appimagetool

# setup env

ENV PATH="/opt/bin:/opt/qt515/bin:${PATH}" \
    PKG_CONFIG_PATH="/opt/qt515/lib/pkgconfig:${PKG_CONFIG_PATH}" \
    LD_LIBRARY_PATH="/opt/qt515/lib:/opt/qt515/lib/x86_64-linux-gnu"

# setup hotspot build environment

FROM hotspot_appimage_intermediate

ARG DISABLE_RUSTC_DEMANGLE
ARG DISABLE_D_DEMANGLE

RUN git clone https://github.com/KDAB/KDDockWidgets ; cd KDDockWidgets ; mkdir build ; cd build ; cmake -DCMAKE_INSTALL_PREFIX=/usr/ .. ; make ; make install

RUN apt-get update && apt-get install -y libphonon4qt5-dev

ADD ./scripts/ /opt/hotspot/scripts/
WORKDIR /opt/hotspot

RUN tar -C / -xvf scripts/elfutils/elfutils.build.tar.bz2
RUN if [ -z "$DISABLE_RUSTC_DEMANGLE" ]; then tar -C / -xvf scripts/rustc-demangle/rustc_demangle.build.tar.bz2; fi
RUN if [ -z "$DISABLE_D_DEMANGLE" ]; then tar -C /opt/lib/ -xvf scripts/d-demangle/d-demangle.tar.gz ; fi

ADD . /opt/hotspot

CMD ./scripts/build_appimage.sh /opt /artifacts
