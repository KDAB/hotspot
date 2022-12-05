# Developing Hotspot - the Linux perf GUI for performance analysis

<img align="right" width="10%" src="src/images/hotspot_logo.png">

## Table of Contents

<!--
    TOC generated with https://github.com/jonschlinkert/markdown-toc
    To update, run `markdown-toc -i README.md`
-->

<!-- toc -->

- [Building Hotspot](#building-hotspot)
  - [Required Dependencies](#required-dependencies)
  - [On Debian/Ubuntu](#on-debianubuntu)
  - [On Fedora](#on-fedora)
  - [Arch Linux](#arch-linux)
  - [OpenSUSE](#opensuse)
  - [Building Hotspot itself](#building-hotspot-itself)
  - [Debugging the AppImage](#debugging-the-appimage)

<!-- tocstop -->

## Building Hotspot

Building hotspot from source gives you the latest and greatest, but you'll have to make sure all its
dependencies are available. Most users should probably [install hotspot](README#getting-hotspot) from the distro
package manager or as an [AppImage](README#for-any-linux-distro-appimage).

### Required Dependencies

As of now, you will need the following dependencies to build this project:

- CMake 3.1.0 or higher
- any C++11 enabled compiler
- Qt 5.15.0 or higher
- libelf
- libelfutils
- gettext
- extra-cmake-modules
- KDE Frameworks 5 (packages are usually called libkf5-*-devel):
  - threadweaver
  - i18n
  - configwidgets
  - coreaddons
  - itemviews
  - itemmodels
  - kio
  - solid
  - windowsystem
  - notifications
  - iconthemes
  - parts
  - archive (optional)
  - kauth (optional)
- [KDDockWidgets](https://github.com/KDAB/KDDockWidgets)
  - this library is not yet packaged on most distributions, you'll have to compile it yourself from source

### On Debian/Ubuntu

```bash
add-apt-repository ppa:kubuntu-ppa/backports
apt-get update
apt-get install libkf5threadweaver-dev libkf5i18n-dev libkf5configwidgets-dev \
    libkf5coreaddons-dev libkf5itemviews-dev libkf5itemmodels-dev libkf5kio-dev libkf5parts-dev \
    libkf5solid-dev libkf5windowsystem-dev libkf5notifications-dev libkf5iconthemes-dev libelf-dev \
    libdw-dev cmake extra-cmake-modules gettext libqt5svg5-dev
```

Note: Ubuntu 20.04 does not provide the minimal version of QT, if you use this old Ubuntu version then
you'd need to _additionally_ install it from a version outside of the distro repository.
The automatic builds via GitHub actions [use the following][Ubuntu2004Script] for that:

```bash
    sudo add-apt-repository ppa:beineri/opt-qt-5.15.4-focal
    sudo apt-get update
    sudo apt-get install -y qt515base qt515svg qt515x11extras \
        libqt5x11extras5-dev   # this one is optional for Hotspot, but required for KDDockWidgets
```

[Ubuntu2004Script]:https://github.com/KDAB/hotspot/blob/master/scripts/compile-test/BaseUbuntu20.04

### On Fedora

```bash
dnf install cmake gcc glibc-static gcc-c++ libstdc++-static qt5-qtbase qt5-qtbase-devel qt5-qtsvg-devel \
    extra-cmake-modules elfutils-devel kf5-threadweaver-devel \
    kf5-kconfigwidgets-devel kf5-kitemviews-devel kf5-kitemmodels-devel \
    kf5-kio-devel kf5-solid-devel kf5-kwindowsystem-devel kf5-kiconthemes-devel \
    kf5-knotifications-devel kf5-kparts-devel
```

### Arch Linux

```bash
pacman -Syu
pacman -S cmake gcc extra-cmake-modules threadweaver kconfigwidgets knotifications \
    kiconthemes kitemviews kitemmodels kwindowsystem kio kparts solid libelf gettext qt5-base
```

### OpenSUSE

```bash
zypper in cmake gcc-c++ extra-cmake-modules threadweaver-devel kio-devel \
    solid-devel kcoreaddons-devel threadweaver-devel kconfigwidgets-devel \
    kitemmodels-devel kitemviews-devel kwindowsystem-devel kparts-devel \
    libqt5-qtbase-devel libqt5-qtsvg-devel libelf-devel libdw-devel gettext glibc-devel-static \
    knotifications-devel kiconthemes-devel libzstd-devel binutils
```

### Building Hotspot itself

```bash
git clone --recurse-submodules https://github.com/KDAB/hotspot.git
mkdir build-hotspot
cd build-hotspot
cmake ../hotspot
make
# now you can run hotspot from the build folder:
./bin/hotspot
# or `make install` it and launch it from your $PATH
```

If you want to use KAuth, you need to add `-DCMAKE_INSTALL_PREFIX=/usr/` to the cmake call. Otherwise KAuth won't work.
If you need help building this project for your platform, [contact us for help](https://www.kdab.com/about/contact/).

### Debugging the AppImage

If you're building Hotspot yourself you have the debugging symbols availalbe, when using a distro version
you can check with your distribution documentation how to install the debug symbols.

But debugging is also possible when using the [AppImage](README#for-any-linux-distro-appimage) as follows:

When the AppImage crashes or is excessively slow, please provide a usable backtrace or profile run.
To do that, you will need to download the debuginfo artifact that matches the AppImage you downloaded.
Next, you will need to download the docker image from `ghcr.io/kdab/kdesrc-build:latest`. For that you
need to authenticate yourself to GitHub. If you have a `ghcr.io` authentication token and use `pass`,
you can do that with

```bash
$ scripts/appimage/download_docker.sh
downloading docker image from ghcr.io
Login Succeeded
latest: Pulling from kdab/kdesrc-build
Digest: sha256:1be0cdbeff1c8c796b77a4244a1f257e02c4a896fd6add93b7b00a9d76db8727
...
ghcr.io/kdab/kdesrc-build:latest
Removing login credentials for ghcr.io
```

Once that is done, you can run the `scripts/appimage/run_debuginfod_in_docker.sh` script as such:

```bash
$ ./scripts/appimage/run_debuginfod_in_docker ~/Downloads/debuginfo.zip
unpacking debuginfo...
hotspot-debuginfo-v1.3.0-391-g590f810/usr/bin/hotspot
hotspot-debuginfo-v1.3.0-391-g590f810/usr/lib64/libexec/hotspot-perfparser
starting debuginfod in docker...
to use it on your host system, set:

    export DEBUGINFOD_URLS="127.0.0.1:12345 https://debuginfod.centos.org/ https://debuginfod.archlinux.org"

[Fri Dec  2 19:58:56 2022] (16/16): opened database /opt/app-root/src/.debuginfod.sqlite
[Fri Dec  2 19:58:56 2022] (16/16): sqlite version 3.7.17
[Fri Dec  2 19:58:56 2022] (16/16): started http server on IPv4 IPv6 port=12345
...
```

Keep that `debuginfod` process running. You can now start using the local debuginfod in combination with
the official Centos instance to get debug information for all the dependencies and build artifacts of hotspot.
Here is one example for that using GDB:

```bash
$ ./hotspot-v1.3.0-391-g590f810-x86_64.AppImage &
$ export DEBUGINFOD_URLS="127.0.0.1:12345 https://debuginfod.centos.org/ https://debuginfod.archlinux.org"
$ gdb -p $(pidof hotspot)
...
This GDB supports auto-downloading debuginfo from the following URLs:
127.0.0.1:12345 https://debuginfod.centos.org/ https://debuginfod.archlinux.org
Enable debuginfod for this session? (y or [n]) y
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
Downloading 37.95 MB separate debug info for /home/milian/Downloads/squashfs-root/usr/bin/hotspot
...
(gdb) bt
#0  0x00007f70bd5140bf in __GI___poll (fds=0x2c83f40, nfds=5, timeout=0) at ../sysdeps/unix/sysv/linux/poll.c:29
#1  0x00007f70b7a4c3fc in g_main_context_iterate.isra () from /tmp/.mount_hotspoBCRbef/usr/lib/libglib-2.0.so.0
#2  0x00007f70b7a4c52c in g_main_context_iteration () from /tmp/.mount_hotspoBCRbef/usr/lib/libglib-2.0.so.0
#3  0x00007f70bdd08aa4 in QEventDispatcherGlib::processEvents(QFlags<QEventLoop::ProcessEventsFlag>) ()
   from /tmp/.mount_hotspoBCRbef/usr/lib/libQt5Core.so.5
#4  0x00007f70bdcb361b in QEventLoop::exec(QFlags<QEventLoop::ProcessEventsFlag>) ()
   from /tmp/.mount_hotspoBCRbef/usr/lib/libQt5Core.so.5
#5  0x00007f70bdcbb26c in QCoreApplication::exec() () from /tmp/.mount_hotspoBCRbef/usr/lib/libQt5Core.so.5
#6  0x000000000043a23e in main (argc=<optimized out>, argv=<optimized out>) at /__w/hotspot/hotspot/src/main.cpp:194
```

Note that source files are not supported in this way yet, as that would blow up the size of the docker image
and thereby slow down our CI.
