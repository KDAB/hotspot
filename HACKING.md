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
- [Online](#online)
- [Debugging the AppImage](#debugging-the-appimage)
  - [AppImage-Debugging using manual symbol loading](#appimage-debugging-using-manual-symbol-loading)
  - [AppImage-Debugging using debuginfod](#appimage-debugging-using-debuginfod)

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
- [KDDockWidgets](https://github.com/KDAB/KDDockWidgets)
  - this library is not yet packaged on most distributions, you'll have to compile it yourself from source

### On Debian/Ubuntu

#### For Qt5

```bash
apt-get update
apt-get install libkf5threadweaver-dev libkf5i18n-dev libkf5configwidgets-dev libkf5syntaxhighlighting-dev \
    libkf5coreaddons-dev libkf5itemviews-dev libkf5itemmodels-dev libkf5kio-dev libkf5parts-dev \
    libkf5solid-dev libkf5windowsystem-dev libkf5notifications-dev libkf5iconthemes-dev libelf-dev \
    libdw-dev cmake extra-cmake-modules gettext libqt5svg5-dev
```

#### For Qt6

```bash
apt-get update
apt-get install libkf6threadweaver-dev libkf6i18n-dev libkf6configwidgets-dev libkf6syntaxhighlighting-dev \
    libkf6coreaddons-dev libkf6itemviews-dev libkf6itemmodels-dev libkf6kio-dev libkf6parts-dev \
    libkf6solid-dev libkf6windowsystem-dev libkf6notifications-dev libkf6iconthemes-dev libelf-dev \
    libdw-dev libdwarf-dev libdebuginfod-dev cmake extra-cmake-modules qt6-svg-dev libzstd-dev \
    libkddockwidgets-qt6-dev kgraphviewer-dev libqcustomplot-dev
```

### On Fedora

```bash
dnf install cmake gcc glibc-static gcc-c++ libstdc++-static qt5-qtbase qt5-qtbase-devel qt5-qtsvg-devel \
    extra-cmake-modules elfutils-devel kf5-threadweaver-devel \
    kf5-kconfigwidgets-devel kf5-kitemviews-devel kf5-kitemmodels-devel kf5-syntax-highlighting-devel \
    kf5-kio-devel kf5-solid-devel kf5-kwindowsystem-devel kf5-kiconthemes-devel \
    kf5-knotifications-devel kf5-kparts-devel
```

### Arch Linux

```bash
pacman -Syu
pacman -S cmake gcc extra-cmake-modules threadweaver kconfigwidgets knotifications syntax-highlighting \
    kiconthemes kitemviews kitemmodels kwindowsystem kio kparts solid libelf gettext qt5-base
```

### OpenSUSE

```bash
zypper in cmake gcc-c++ extra-cmake-modules threadweaver-devel kio-devel \
    solid-devel kcoreaddons-devel threadweaver-devel kconfigwidgets-devel \
    kitemmodels-devel kitemviews-devel kwindowsystem-devel kparts-devel syntax-highlighting-devel \
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

To build for Qt6 pass `-DQT6_BUILD=ON` when running the `cmake` command.

If you need help building this project for your platform, [contact us for help](https://www.kdab.com/about/contact/).

## Online

Instead of installing all dependencies and building Hotspot locally you may do so online with the following link:
[![Open in Gitpod](https://gitpod.io/button/open-in-gitpod.svg)](https://gitpod.io#https://github.com/GitMensch/hotspot)
This environment uses a clean, pre-configured Ubuntu 22.04 LTS environment. Just open, possibly
have a look at the multiple building tasks, wait a bit until building is finished which is done
as soon as a hotspot window opens in the preview and you can start coding / debugging hotspot.

Notes:

- This Gitpod setup is maintained by @GitMensch and not used by @KDAB directly.
- Building may take a while as we don't use pre-builts or a special docker container in this environment yet.

## Debugging the AppImage

When the AppImage crashes or is excessively slow, please provide a usable backtrace or profile run.

If you're building Hotspot yourself you have the debugging symbols availalbe, when using a distro version
you can check with your distribution documentation how to install the debug symbols.

But debugging is also possible when using the [AppImage](README#for-any-linux-distro-appimage) using debuginfod or
manual symbol loading.

To do that, you will need to download the debuginfo artifact that matches the AppImage you downloaded.

### AppImage-Debugging using manual symbol loading

For using this approach you need to first unpack the debug information
downloaded from the release:

```bash
$ tar -xvf hotspot-debuginfo-*.tar.bz2
hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot
hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/lib64/libexec/hotspot-perfparser
```

Starting the debugger is possible directly with executing the AppImage as follows:

```bash
$ ./hotspot-v1.2.3-45-g02fd82c-x86_64.AppImage &
$ gdb -q -p $(pidof hotspot) -ex "add-symbol-file hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot"
Attaching to process 123456
```

with a GDB session of

```txt
Reading symbols from /tmp/.mount_hotspoBCRbef/squashfs-root/usr/bin/hotspot...

(No debugging symbols found in /tmp/.mount_hotspoBCRbef/squashfs-root/usr/bin/hotspot)
Reading symbols from /tmp/.mount_hotspoBCRbef/squashfs-root/usr/bin/../lib/libKF5ThreadWeaver.so.5...
(No debugging symbols found in /tmp/.mount_hotspoBCRbef/squashfs-root/usr/bin/../lib/libKF5ThreadWeaver.so.5)
...
0x00007fea98ae0a38 in poll () from /usr/lib64/libc.so.6
add symbol table from file "hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot"
(y or n) y
Reading symbols from hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot...
(gdb) bt
#0  0x00007ff72544ca38 in poll () from /usr/lib64/libc.so.6
#1  0x00007ff7246777d7 in _xcb_in_read_block () from /usr/lib64/libxcb.so.1
#2  0x00007ff7246754e9 in xcb_connect_to_fd () from /usr/lib64/libxcb.so.1
#3  0x00007ff72467943c in xcb_connect_to_display_with_auth_info () from /usr/lib64/libxcb.so.1
#4  0x00007ff7283a95ea in _XConnectXCB () from /usr/lib64/libX11.so.6
#5  0x00007ff728399d89 in XOpenDisplay () from /usr/lib64/libX11.so.6
#6  0x00007ff70f181527 in QXcbBasicConnection::QXcbBasicConnection(char const*) () from /tmp/squashfs-root/usr/plugins/platforms/../../lib/libQt5XcbQpa.so.5
#7  0x00007ff70f15b072 in QXcbConnection::QXcbConnection(QXcbNativeInterface*, bool, unsigned int, char const*) ()
   from /tmp/squashfs-root/usr/plugins/platforms/../../lib/libQt5XcbQpa.so.5
#8  0x00007ff70f15e588 in QXcbIntegration::QXcbIntegration(QStringList const&, int&, char**) ()
   from /tmp/squashfs-root/usr/plugins/platforms/../../lib/libQt5XcbQpa.so.5
#9  0x00007ff7286ad41f in ?? () from /tmp/squashfs-root/usr/plugins/platforms/libqxcb.so
#10 0x00007ff7267686cf in QGuiApplicationPrivate::createPlatformIntegration() () from /tmp/squashfs-root/usr/bin/../lib/libQt5Gui.so.5
#11 0x00007ff726769c40 in QGuiApplicationPrivate::createEventDispatcher() () from /tmp/squashfs-root/usr/bin/../lib/libQt5Gui.so.5
#12 0x00007ff72630c0d7 in QCoreApplicationPrivate::init() () from /tmp/squashfs-root/usr/bin/../lib/libQt5Core.so.5
#13 0x00007ff72676c639 in QGuiApplicationPrivate::init() () from /tmp/squashfs-root/usr/bin/../lib/libQt5Gui.so.5
#14 0x00007ff726e12a69 in QApplicationPrivate::init() () from /tmp/squashfs-root/usr/bin/../lib/libQt5Widgets.so.5
#15 0x000000000043a929 in main (argc=<optimized out>, argv=<optimized out>) at /__w/hotspot/hotspot/src/main.cpp:69
(gdb) continue

```

or with an extracted image:

```bash
$ ./hotspot-v1.2.3-45-g02fd82c-x86_64.AppImage --appimage-extract
$ PATH=./squashfs-root/usr/bin:$PATH LD_LIBRARY_PATH=./squashfs-root/usr/lib64:/usr/local/lib64:/usr/lib64:./squashfs-root/usr/lib:$LD_LIBRARY_PATH gdb -q --exec=hotspot --symbols hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot -ex run
Reading symbols from hotspot-debuginfo-v1.2.3-45-g02fd82c/usr/bin/hotspot...
Starting program: /tmp/squashfs-root/usr/bin/hotspot

```

Note that source files matching the commit the AppImage was created from needs to be manually downloaded
or inspected in the git repo for this approach. Also note that you cannot debug any QT (or other system) parts
this way, only Hotspot itself.

### AppImage-Debugging using debuginfod

To use debuginfod, you will need to additionally download the docker image from `ghcr.io/kdab/kdesrc-build:latest`.
For that you need to authenticate yourself to GitHub. If you have a `ghcr.io` authentication token and
use `pass`, you can do that with

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

[Fri May 24 19:58:56 2024] (16/16): opened database /opt/app-root/src/.debuginfod.sqlite
[Fri May 24 19:58:56 2024] (16/16): sqlite version 3.7.17
[Fri May 24 19:58:56 2024] (16/16): started http server on IPv4 IPv6 port=12345
...
```

Keep that `debuginfod` process running. You can now start using the local debuginfod in combination with
the official Centos instance to get debug information for all the dependencies and build artifacts of hotspot.
Here is one example for that using GDB:

```bash
$ ./hotspot-v1.2.3-45-g02fd82c-x86_64.AppImage &
$ export DEBUGINFOD_URLS="127.0.0.1:12345 https://debuginfod.centos.org/ https://debuginfod.archlinux.org"
$ gdb -q -p $(pidof hotspot)
Attaching to process 123456
...
```

with a GDB session of

```txt
...
This GDB supports auto-downloading debuginfo from the following URLs:
127.0.0.1:12345 https://debuginfod.centos.org/ https://debuginfod.archlinux.org
Enable debuginfod for this session? (y or [n]) y
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
Downloading 37.95 MB separate debug info for /home/user/Downloads/squashfs-root/usr/bin/hotspot
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
(gdb) continue
```

Note that source files are not supported in this way yet, as that would blow up the size of the docker image
and thereby slow down our CI.
