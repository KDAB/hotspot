#
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
FROM centos/devtoolset-7-toolchain-centos7 as intermediate

ARG QT_VERSION=5.15.7
ARG KDDockWidgets_VERSION=1.6
ARG QCustomPlot_VERSION=2.1.1
ARG rust_demangler_VERSION=0.1.21
ARG d_demangler_VERSION=0.0.2

USER root

# install dependencies
RUN sed -i 's#enabled=1#enabled=0#' /etc/yum/pluginconf.d/fastestmirror.conf && \
    yum install -y deltarpm && \
    yum update -y && yum install -y epel-release centos-release-scl && \
    yum install -y \
        devtoolset-11 devtoolset-11-elfutils-devel devtoolset-11-elfutils-debuginfod flex bison file \
        rh-perl530-perl rh-perl530-perl-IO-Socket-SSL rh-perl530-perl-YAML \
        gperf wget cmake3 which rh-git227-git python3 libzstd-devel \
        polkit-devel libxslt-devel docbook-style-xsl \
        # install Qt build dependencies, see: https://wiki.qt.io/Building_Qt_5_from_Git
        # qtbase
        libxcb libxcb-devel xcb-util xcb-util-devel mesa-libGL-devel libxkbcommon-devel libudev-devel \
        xcb-util-keysyms-devel libxkbcommon-x11-devel libinput-devel xcb-util-image-devel \
        mesa-libgbm-devel xcb-util-wm-devel xcb-util-renderutil-devel libSM-devel \
        postgresql-devel unixODBC-devel pcre2-devel openssl11-devel openssl11 cups-devel \
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
        fuse fuse-libs bzip2 && \
    ln -s /usr/bin/cmake3 /usr/bin/cmake && \
    rm -Rf /var/cache/yum && \
    . /opt/rh/devtoolset-11/enable

ENV LD_LIBRARY_PATH=/opt/rh/devtoolset-11/root/usr/lib64:/opt/rh/devtoolset-11/root/usr/lib:/opt/rh/devtoolset-11/root/usr/lib64/dyninst:/opt/rh/devtoolset-11/root/usr/lib/dyninst:/opt/rh/devtoolset-11/root/usr/lib64:/opt/rh/devtoolset-11/root/usr/lib:/opt/rh/httpd24/root/usr/lib64:/opt/rh/rh-perl530/root/usr/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}} \
    PATH=/opt/rh/devtoolset-11/root/usr/bin:/opt/rh/rh-git227/root/usr/bin:/opt/rh/rh-perl530/root/usr/local/bin:/opt/rh/rh-perl530/root/usr/bin${PATH:+:${PATH}}

# qt5
RUN wget https://download.qt.io/official_releases/qt/5.15/${QT_VERSION}/single/qt-everywhere-opensource-src-${QT_VERSION}.tar.xz && \
    tar -xvf qt-everywhere-opensource-src-${QT_VERSION}.tar.xz && \
    mkdir build && cd build && \
    ../qt-everywhere-src-${QT_VERSION}/configure -opensource -confirm-license \
    -xcb -xcb-xlib -bundled-xcb-xinput -skip qtwebengine -skip qtdatavis3d -skip qtmultimedia -skip qtquick3d \
    -skip qtquickcontrols -skip qtquickcontrols2 -skip qtquicktimeline -skip qt3d -skip qtdeclarative \
    -skip qtscript -skip qtcharts -skip qtdoc -skip qtwebsockets -skip qtgamepad -skip qtgraphicaleffects \
    -skip qtpurchasing -skip qtserialbus -skip qtserialport -skip qtwebchannel -skip qtwebglplugin \
    -skip qtwebview -skip qtlocation -skip qtspeech -skip qtwinextras \
    -release -ssl -no-compile-examples -cups -I /usr/include/openssl11 -prefix /usr && \
    make -j && make install && cd .. && rm -Rf build qt-everywhere-*

# appimage build tools
RUN wget https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20220822-1/linuxdeploy-x86_64.AppImage &&     chmod +x linuxdeploy-x86_64.AppImage && \
    wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage && chmod +x linuxdeploy-plugin-qt-x86_64.AppImage && mv linuxdeploy* /usr/bin/

# qcustomplot
RUN cd /opt && mkdir qcustomplot && cd qcustomplot && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot.tar.gz && tar -xf QCustomPlot.tar.gz && \
    cd qcustomplot && cp qcustomplot.h /usr/include && \
    wget https://www.qcustomplot.com/release/${QCustomPlot_VERSION}/QCustomPlot-sharedlib.tar.gz && \
    tar -xf QCustomPlot-sharedlib.tar.gz && ls && cd qcustomplot-sharedlib/sharedlib-compilation \
    && qmake . && make -j && mv libqcustomplot.so* /usr/lib && cd /opt && rm -Rf qcustomplot && ldconfig

# rust demangle
RUN cd /opt/ && mkdir rust && cd rust && wget https://sh.rustup.rs -O rustup.sh && \
    chmod +x rustup.sh && ./rustup.sh -y && \
    git clone -b ${rust_demangler_VERSION} https://github.com/rust-lang/rustc-demangle && \
    cd rustc-demangle && $HOME/.cargo/bin/cargo build -p rustc-demangle-capi --release && \
    cp target/release/librustc_demangle.so /usr/lib/ && \
    cd /opt && rm -Rf /opt/rust $HOME/.cargo $HOME/.rustup

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
    deactivate && cd /opt && rm -Rf dlang && mv /usr/bin/gpg{~,} && mv /usr/bin/gpg2{~,}

# kde frameworks
RUN cd /opt && git clone https://invent.kde.org/sdk/kdesrc-build.git && \
    # everything after this commit requires a dbus session
    cd kdesrc-build && git checkout 6c2c8a83ee25d53bdae91dce4ab3efaaac838a11
COPY kdesrc-buildrc /opt/kdesrc-build/
RUN ldconfig && \
    cd /opt/kdesrc-build && sed -i -e "s/FULL_NPROC/$(nproc)/g" -e "s/HALF_NPROC/$(($(nproc)/2))/g" kdesrc-buildrc && \
    cpan YAML::Syck && \
    ./kdesrc-build --metadata-only && \
    # fix compile error in kwallet
    ./kdesrc-build --include-dependencies qca && \
    ./kdesrc-build --include-dependencies threadweaver kconfigwidgets kcoreaddons kitemviews kitemmodels solid \
        kwindowsystem knotifications kiconthemes karchive syntax-highlighting kio kparts && \
    # without extra dependencies
    ./kdesrc-build kgraphviewer && \
    cd /opt && rm -Rf kdesrc-build kde && ldconfig

# kddockwidgets
RUN cd /opt && git clone --recursive https://github.com/KDAB/KDDockWidgets.git -b ${KDDockWidgets_VERSION} && \
    cd KDDockWidgets && mkdir build && cd build && cmake -DCMAKE_INSTALL_PREFIX=/usr .. && \
    make -j && make install && cd /opt && rm -Rf KDDockWidgets

FROM intermediate

WORKDIR /
RUN mkdir /output
