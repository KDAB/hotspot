#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
FROM almalinux:8 AS intermediate

ARG QT_VERSION=5.15.14
ARG KF5_VERSION=v5.115.0
ARG KDDockWidgets_VERSION=2.1
ARG QCustomPlot_VERSION=2.1.1
ARG rust_demangler_VERSION=0.1.23
ARG d_demangler_VERSION=0.0.2
ARG KGraphViewer_VERSION=v2.4.3a

USER root

# install dependencies
RUN dnf update -y && dnf install -y 'dnf-command(config-manager)' && dnf config-manager --set-enabled powertools && dnf update -y && \
    dnf install -y \
        gcc-toolset-13 elfutils-devel elfutils-libelf-devel elfutils-debuginfod-client-devel flex bison file \
        perl perl-IO-Socket-SSL perl-YAML \
        gperf wget cmake3 which git python3 libzstd-devel \
        polkit-devel libxslt-devel docbook-style-xsl \
        # install Qt build dependencies, see: https://wiki.qt.io/Building_Qt_5_from_Git
        # qtbase
        libxcb libxcb-devel xcb-util xcb-util-devel mesa-libGL-devel libxkbcommon-devel libudev-devel \
        xcb-util-keysyms-devel libxkbcommon-x11-devel libinput-devel xcb-util-image-devel \
        mesa-libgbm-devel xcb-util-wm-devel xcb-util-renderutil-devel libSM-devel \
        postgresql-devel unixODBC-devel pcre2-devel openssl-devel openssl cups-devel \
        # gtk widget theme support
        gtk3-devel \
        # kshimgen
        glibc-static \
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
        libmicrohttpd-devel sqlite-devel libarchive-devel && \
    rm -Rf /var/dnf/yum

FROM intermediate AS builder

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib:/opt/rh/gcc-toolset-13/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib
ENV PATH=/opt/rh/gcc-toolset-13/root/usr/bin${PATH:+:${PATH}}

RUN . /opt/rh/gcc-toolset-13/enable && wget https://download.qt.io/official_releases/qt/5.15/${QT_VERSION}/single/qt-everywhere-opensource-src-${QT_VERSION}.tar.xz && \
    tar -xvf qt-everywhere-opensource-src-${QT_VERSION}.tar.xz && \
    mkdir build && cd build && \
    ../qt-everywhere-src-${QT_VERSION}/configure -opensource -confirm-license -force-debug-info -separate-debug-info \
    -xcb -xcb-xlib -bundled-xcb-xinput -skip qtwebengine -skip qtdatavis3d -skip qtmultimedia -skip qtquick3d \
    -skip qtquickcontrols -skip qtquickcontrols2 -skip qtquicktimeline -skip qt3d -skip qtdeclarative \
    -skip qtscript -skip qtcharts -skip qtdoc -skip qtwebsockets -skip qtgamepad -skip qtgraphicaleffects \
    -skip qtpurchasing -skip qtserialbus -skip qtserialport -skip qtwebchannel -skip qtwebglplugin \
    -skip qtwebview -skip qtlocation -skip qtspeech -skip qtwinextras \
    -release -ssl -no-compile-examples -cups -I /usr/include/openssl11 -prefix /usr -linker gold && \
    make -j$(nproc) && make install

# qcustomplot
RUN cd /opt && mkdir qcustomplot && cd qcustomplot && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot.tar.gz && tar -xf QCustomPlot.tar.gz && \
    cd qcustomplot && cp qcustomplot.h /usr/include && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot-sharedlib.tar.gz && \
    tar -xf QCustomPlot-sharedlib.tar.gz && ls && cd qcustomplot-sharedlib/sharedlib-compilation \
    && qmake . && make -j$(nproc) && mv libqcustomplot.so* /usr/lib

# rust demangle
RUN cd /opt/ && mkdir rust && cd rust && wget https://sh.rustup.rs -O rustup.sh && \
    chmod +x rustup.sh && ./rustup.sh -y && \
    git clone -b ${rust_demangler_VERSION} https://github.com/rust-lang/rustc-demangle && \
    cd rustc-demangle && $HOME/.cargo/bin/cargo build -p rustc-demangle-capi --release && \
    cp target/release/librustc_demangle.so /usr/lib/

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
    deactivate && mv /usr/bin/gpg{~,} && mv /usr/bin/gpg2{~,}

# kdesrc-build dependencies
RUN yum install -y libcanberra-devel gpgme-devel && \
    cpan YAML::Syck JSON::XS YAML::XS

# kde frameworks
RUN cd /opt && git clone https://invent.kde.org/sdk/kdesrc-build.git
COPY kdesrc-buildrc /opt/kdesrc-build/
RUN ldconfig && \
    cd /opt/kdesrc-build && \
    sed -i -e "s/FULL_NPROC/$(nproc)/g" \
        -e "s/HALF_NPROC/$(($(nproc)/2))/g" -e "s/%VERSION%/${KF5_VERSION}/g" kdesrc-buildrc && \
    # workaround for https://bugs.kde.org/show_bug.cgi?id=490168
    sed -i "s/ignore-modules|!=/ignore-modules|=/" modules/ksb/Cmdline.pm && \
    ./kdesrc-build --metadata-only && \
    # fix compile error in kwallet
    ./kdesrc-build --include-dependencies qca && \
    ./kdesrc-build --ignore-modules=gpgme --include-dependencies threadweaver kconfigwidgets kcoreaddons \
        kitemviews kitemmodels solid breeze-icons \
        kwindowsystem knotifications kiconthemes karchive syntax-highlighting kio kparts && \
    # without extra dependencies
    ./kdesrc-build --revision ${KGraphViewer_VERSION} --no-include-dependencies kgraphviewer

# kddockwidgets
RUN cd /opt && git clone --recursive https://github.com/KDAB/KDDockWidgets.git -b ${KDDockWidgets_VERSION} && \
    cd KDDockWidgets && mkdir build && cd build && cmake -DCMAKE_INSTALL_PREFIX=/usr -DKDDockWidgets_EXAMPLES=0 -DKDDockWidgets_FRONTENDS="qtwidgets" .. && \
    make -j$(nproc) && make install

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

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib:/opt/rh/gcc-toolset-13/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib:/opt/rh/rh-perl530/root/usr/lib64:/usr/lib
ENV PATH=/opt/rh/gcc-toolset-13/root/usr/bin:/opt/rh/rh-git227/root/usr/bin:/opt/rh/rh-perl530/root/usr/local/bin:/opt/rh/rh-perl530/root/usr/bin${PATH:+:${PATH}}

RUN git config --global --add safe.directory /github/workspace

COPY --from=stripped /usr /usr
COPY --from=stripped /usr/local/lib64 /opt/rh/gcc-toolset-13/root/lib64

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

ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib:/opt/rh/gcc-toolset-13/root/usr/lib64/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib/dyninst:/opt/rh/gcc-toolset-13/root/usr/lib64:/opt/rh/gcc-toolset-13/root/usr/lib:/usr/lib
ENV PATH=/opt/rh/gcc-toolset-13/root/usr/bin${PATH:+:${PATH}}

COPY --from=debugcollector /qt-debuginfo-x86_64.tar.bz2 /
COPY --from=debugcollector /gh_2.49.2_linux_amd64/bin/gh /usr/bin/
COPY publish-qt-debuginfo.sh /

CMD ["/publish-qt-debuginfo.sh"]
