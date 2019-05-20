# trusty
FROM ubuntu:14.04 as hotspot_appimage_intermediate

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y software-properties-common build-essential curl git wget \
        autotools-dev autoconf libtool liblzma-dev libz-dev gettext && \
    add-apt-repository ppa:beineri/opt-qt-5.10.1-trusty -y && \
    apt-get update && \
    apt-get install -y qt510base qt510svg qt510x11extras cmake3 libdwarf-dev mesa-common-dev \
        libboost-iostreams-dev libboost-program-options-dev libboost-system-dev libboost-filesystem-dev

WORKDIR /opt

# download prebuild KF5 libraries and ECM

RUN wget -c "https://github.com/chigraph/precompiled-kf5-linux/releases/download/precompiled/kf5-gcc6-linux64-release.tar.xz" && \
    tar --strip-components=2 -xf kf5-gcc6-linux64-release.tar.xz -C /opt/qt510/

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

ENV PATH="/opt/bin:/opt/qt510/bin:${PATH}" \
    PKG_CONFIG_PATH="/opt/qt510/lib/pkgconfig:${PKG_CONFIG_PATH}" \
    LD_LIBRARY_PATH="/opt/qt510/lib:/opt/qt510/lib/x86_64-linux-gnu"

# setup hotspot build environment

FROM hotspot_appimage_intermediate

ADD . /opt/hotspot
WORKDIR /opt/hotspot

RUN tar -C / -xvf scripts/elfutils/elfutils.build.tar.bz2

CMD ./scripts/build_appimage.sh /opt /artifacts
