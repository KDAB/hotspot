#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
FROM almalinux:9 AS intermediate

ARG QT_VERSION=6.8.3
ARG QT_VERSION_SHORT=6.8
ARG KF6_VERSION=v6.16.0
ARG KDDockWidgets_VERSION=v2.2.5
ARG QCustomPlot_VERSION=2.1.1
ARG rust_demangler_VERSION=0.1.24
ARG d_demangler_VERSION=0.0.2
ARG KGraphViewer_VERSION=v25.04.3
ARG KDiagram_VERSION=v3.0.1

USER root

# install dependencies
RUN dnf update -y && dnf install -y 'dnf-command(config-manager)' && dnf config-manager --set-enabled crb &&  dnf update -y && \
    dnf install -y \
        glibc-langpack-en gcc-toolset-14 elfutils-devel elfutils-libelf-devel elfutils-debuginfod-client-devel flex bison file \
        perl perl-IO-Socket-SSL perl-YAML \
        gperf wget cmake3 which git python3 libzstd-devel \
        polkit-devel libxslt-devel docbook-style-xsl \
        # install Qt build dependencies, see: https://wiki.qt.io/Building_Qt_6_from_Git
        # qtbase
        libxcb libxcb-devel xcb-util xcb-util-devel mesa-libGL-devel libxkbcommon-devel libudev-devel \
        libinput-devel xcb-util-image-devel xcb-util-wm-devel xcb-util-keysyms-devel xcb-util-cursor-devel \
        mesa-libgbm-devel xcb-util-renderutil-devel libSM-devel libxkbcommon-x11-devel \
        postgresql-devel unixODBC-devel pcre2-devel openssl-devel openssl cups-devel \
        # gtk widget theme support
        gtk3-devel libcanberra-devel \
        # kfilemetadata
        libattr-devel \
        # kio
        libmount-devel \
        # debugging
        nano \
        # kgraphviewer
        boost boost-devel graphviz-devel \
        # appimages
        fuse fuse-libs bzip2 rsync \
        # elfutils
        libmicrohttpd-devel sqlite-devel libarchive-devel \
        # wayland
        ninja-build meson expat-devel doxygen xmlto \
        # sonnet
        hunspell-devel \
        # kwallet
        gpgme-devel gpgmepp-devel libgcrypt-devel libsecret-devel && \
    rm -Rf /var/dnf/yum

FROM intermediate AS builder

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib:/opt/rh/gcc-toolset-14/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib
ENV PATH=/opt/rh/gcc-toolset-14/root/usr/bin${PATH:+:${PATH}}

# Set environment variables for locale
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

RUN . /opt/rh/gcc-toolset-14/enable

# qt
RUN cd /opt && mkdir qt && cd qt && wget https://download.qt.io/official_releases/qt/${QT_VERSION_SHORT}/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz && \
    tar -xvf qt-everywhere-src-${QT_VERSION}.tar.xz && \
    mkdir build && cd build && ../qt-everywhere-src-${QT_VERSION}/configure -opensource -confirm-license -force-debug-info -separate-debug-info \
    -xcb -xcb-xlib -bundled-xcb-xinput \
    -skip qtwebengine,qtwebsockets,qtwebchannel,qtquick3d,qtdatavis3d,qt3d,qtdeclarative \
    -skip qtmultimedia,qtquicktimeline,qtgraphs,qtlottie \
    -skip qtcharts,qtdoc,qtserialbus \
    -skip qtserialport,qtwebview,qtlocation,qtspeech,qtgraphs \
    -skip qtmqtt,qtopcua,qtquick3dphysics,qtquickeffectmaker,qtvirtualkeyboard,qtscxml \
    -skip qtremoteobjects,qtsensors,qtpositioning,qthttpserver,qtgrpc,qtconnectivity,qtcoap \
    -skip qtactiveqt,qtlanguageserver,qtshadertools,qtnetworkauth \
    -release -ssl -nomake examples -cups -I /usr/include/openssl11 -prefix /usr -linker gold && \
    ninja install && cd /opt && rm -Rf qt

# qcustomplot
RUN cd /opt && mkdir qcustomplot && cd qcustomplot && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot.tar.gz && tar -xf QCustomPlot.tar.gz && \
    cd qcustomplot && cp qcustomplot.h /usr/include && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot-sharedlib.tar.gz && \
    tar -xf QCustomPlot-sharedlib.tar.gz && ls && cd qcustomplot-sharedlib/sharedlib-compilation \
    && qmake . && make -j$(nproc) && mv libqcustomplot.so* /usr/lib && cd /opt && rm -Rf qcustomplot

# rust demangle
RUN cd /opt/ && mkdir rust && cd rust && wget https://sh.rustup.rs -O rustup.sh && \
    chmod +x rustup.sh && ./rustup.sh -y && \
    git clone -b ${rust_demangler_VERSION} https://github.com/rust-lang/rustc-demangle && \
    cd rustc-demangle && $HOME/.cargo/bin/cargo build -p rustc-demangle-capi --release && \
    cp target/release/librustc_demangle.so /usr/lib/ && cd /opt && rm -Rf rust

# dlang demangle
RUN cd /opt && mkdir dlang && cd dlang && \
    # gpg is too old and cannot verify the signature, disable it temporarily
    mv /usr/bin/gpg{,~} && mv /usr/bin/gpg2{,~} && \
    wget https://dlang.org/install.sh && chmod +x install.sh && ./install.sh -p /opt/dlang && \
    source /opt/dlang/dmd-*/activate && \
    wget https://github.com/lievenhey/d_demangler/archive/refs/tags/version-${d_demangler_VERSION}.tar.gz -O - | tar -xz && \
    cd d_demangler-version-${d_demangler_VERSION}/ && \
    # link libphobos2 statically
    sed -i 's/defaultlib=libphobos2.so/defaultlib=:libphobos2.a/' Makefile && \
    make && mv libd_demangle.so /usr/lib/ && \
    deactivate && mv /usr/bin/gpg{~,} && mv /usr/bin/gpg2{~,} && \
    cd /opt && rm -Rf dlang

# kdesrc-build dependencies
RUN cpan YAML::Syck JSON::XS YAML::XS

# kde frameworks
RUN cd /opt && git clone https://invent.kde.org/sdk/kdesrc-build.git
COPY kdesrc-buildrc /opt/kdesrc-build/

RUN ldconfig && \
    cd /opt/kdesrc-build && \
    sed -i -e "s/FULL_NPROC/$(nproc)/g" \
        -e "s/HALF_NPROC/$(($(nproc)/2))/g" -e "s/%VERSION%/${KF6_VERSION}/g" kdesrc-buildrc && \
    ./kdesrc-build --metadata-only && \
    # fix compile error in kwallet
    ./kdesrc-build --include-dependencies qca && \
    ./kdesrc-build --ignore-modules=gpgme --include-dependencies threadweaver kconfigwidgets kcoreaddons \
        kitemviews kitemmodels solid breeze-icons \
        kwindowsystem knotifications kiconthemes karchive syntax-highlighting kio kparts && \
    # without extra dependencies
    ./kdesrc-build --revision ${KGraphViewer_VERSION} --no-include-dependencies kgraphviewer && \
    ./kdesrc-build --revision ${KDiagram_VERSION} --no-include-dependencies kdiagram && \
    cd /opt && rm -Rf /opt/kdesrc-build/

# kddockwidgets
RUN cd /opt && mkdir kddw && cd kddw && git clone --recursive https://github.com/KDAB/KDDockWidgets.git -b ${KDDockWidgets_VERSION} && \
    cd KDDockWidgets && mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DKDDockWidgets_EXAMPLES=0 -DKDDockWidgets_FRONTENDS="qtwidgets" -DKDDockWidgets_QT6=ON -G Ninja .. && \
    ninja install && cd /opt && rm -Rf kddw

FROM builder AS stripped

WORKDIR /

RUN find /usr -type f -name "*.debug" -exec rm {} \;
RUN find /usr -type f -name "libKF*" -exec strip --strip-all {} \;
RUN find /usr -type f -name "libQt*" -exec strip --strip-all {} \;
RUN find /usr -type f -name "libkd*" -exec strip --strip-all {} \;
RUN find /usr/local -type f -name "*.so*" -exec strip --strip-all {} \;

RUN cd /opt && wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && chmod +x linuxdeploy-x86_64.AppImage && ./linuxdeploy-x86_64.AppImage --appimage-extract && \
    wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage && chmod +x linuxdeploy-plugin-qt-x86_64.AppImage && ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract && \
    wget https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage && chmod +x linuxdeploy-plugin-appimage-x86_64.AppImage && ./linuxdeploy-plugin-appimage-x86_64.AppImage --appimage-extract && \
    cd squashfs-root && rsync -a usr/ /usr && rsync -a plugins/linuxdeploy-plugin-appimage/ / && cd /opt/squashfs-root/appimagetool-prefix && rsync -a usr/ /usr

FROM intermediate AS kdesrc-build

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib:/opt/rh/gcc-toolset-14/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib:/opt/rh/rh-perl530/root/usr/lib64:/usr/lib
ENV PATH=/opt/rh/gcc-toolset-14/root/usr/bin:/opt/rh/rh-git227/root/usr/bin:/opt/rh/rh-perl530/root/usr/local/bin:/opt/rh/rh-perl530/root/usr/bin${PATH:+:${PATH}}

# Set environment variables for locale
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

COPY --from=stripped /usr /usr
COPY --from=stripped /usr/local/lib64 /opt/rh/gcc-toolset-14/root/lib64

# set the entrypoint to the build script so that the build script will be run by github actions
CMD ["/github/workspace/scripts/appimage/build_appimage.sh", "/github/workspace", "/github/workspace/build"]

FROM builder AS debugcollector

RUN mkdir /debug
RUN find /usr -type f -name "libKF*" -execdir strip --only-keep-debug {} /debug/{}.debug \;
RUN find /usr -type f -name "libQ*.debug" -execdir cp {} /debug/{} \;
RUN find /usr -type f -name "libq*.debug" -execdir cp {} /debug/{} \;
RUN find /usr -type f -name "libkd*" -execdir strip --only-keep-debug {} /debug/{}.debug \;
RUN find /usr/local -type f -name "*.so*" -execdir strip --only-keep-debug {} /debug/{}.debug \;

RUN tar -cjvf "/qt-debuginfo-x86_64.tar.bz2" /debug
RUN cd / &&  wget https://github.com/cli/cli/releases/download/v2.49.2/gh_2.49.2_linux_amd64.tar.gz && tar -xzf gh_2.49.2_linux_amd64.tar.gz

FROM intermediate AS debuginfo

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib:/opt/rh/gcc-toolset-14/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib/dyninst:/opt/rh/gcc-toolset-14/root/usr/lib64:/opt/rh/gcc-toolset-14/root/usr/lib:/usr/lib
ENV PATH=/opt/rh/gcc-toolset-14/root/usr/bin${PATH:+:${PATH}}

# Set environment variables for locale
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

COPY --from=debugcollector /qt-debuginfo-x86_64.tar.bz2 /
COPY --from=debugcollector /gh_2.49.2_linux_amd64/bin/gh /usr/bin/
COPY publish-qt-debuginfo.sh /

CMD ["/publish-qt-debuginfo.sh"]
