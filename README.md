# Hotspot

<img align="right" width="10%" src="src/images/hotspot_logo.png">

The Qt GUI for performance analysis.

This project is a [KDAB](http://www.kdab.com) R&D effort to create a standalone
GUI for performance data. As the first goal, we want to provide a UI like
KCachegrind around Linux perf. Looking ahead, we intend to support various other
performance data formats under this umbrella.

## Table of Contents

   * [Hotspot](#hotspot)
      * [Screenshots](#screenshots)
      * [Dependencies](#dependencies)
      * [Building](#building)
      * [Using](#using)
         * [Embedded Systems](#embedded-systems)
      * [Known Issues](#known-issues)
         * [Broken Backtraces](#broken-backtraces)
         * [Missing Features](#missing-features)
      * [Qt Creator](#qt-creator)

## Screenshots

Here are some screenshots showing some of the hotspot features in action:

![hotspot summary page](screenshots/summary.png?raw=true "hotspot summary page")

![hotspot FlameGraph page](screenshots/flamegraph.png?raw=true "hotspot FlameGraph page")

![hotspot caller-callee page](screenshots/caller-callee.png?raw=true "hotspot caller-callee page")

![hotspot bottom-up page](screenshots/bottom-up.png?raw=true "hotspot bottom-up page")

![hotspot top-down page](screenshots/top-down.png?raw=true "hotspot top-down page")

## Dependencies

As of now, you will need the following dependencies to build this project:

- CMake 3.1.0 or higher
- any C++14 enabled compiler
- Qt 5.6.0 or higher
- libelf
- libelfutils

## Building

```
git clone git@github.com:KDAB/hotspot.git
mkdir build-hotspot
cd build-hotspot
cmake ../hotspot
make
# now you can run hotspot from the build folder:
./bin/hotspot
# or `make install` it and launch it from your $PATH
```

If you need help building this project for your platform, [contact us for help](https://www.kdab.com/about/contact/).

## Using

First of all, record some data with `perf`. To get backtraces, you will need to enable the dwarf callgraph mode:

```
perf record --call-graph dwarf <your application>
...
[ perf record: Woken up 58 times to write data ]
[ perf record: Captured and wrote 14.874 MB perf.data (1865 samples) ]
```

Now, if you have hotspot available on the same machine, all you need to do is launch it.
It will automatically open the `perf.data` file in the current directory (similar to `perf report`).
Alternatively, you can specify the path to the data file on the console:

```
hotspot /path/to/perf.data
```

### Embedded Systems

If you are recording on an embedded system, you will want to analyze the data on your
development machine with hotspot. To do so, make sure your sysroot contains the debug
information required for unwinding (see below). Then record the data on your embedded
system:

```
embedded$ perf record --call-graph dwarf <your application>
...
[ perf record: Woken up 58 times to write data ]
[ perf record: Captured and wrote 14.874 MB perf.data (1865 samples) ]
embedded$ cp /proc/kallsyms /tmp/kallsyms # make pseudo-file a real file
```

It's OK if your embedded machine is using a different platform than your host. On your
host, do the following steps then to analyze the data:

```
host$ scp embedded:perf.data embedded:/tmp/kallsyms .
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms \
              perf.data
```

If you manually deployed an application from a path outside your sysroot, do this instead:

```
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              perf.data
```

If your application is also using libraries outside your sysroot and the appPath, do this:

```
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              --extraLibPaths /path/to/lib1:/path/to/lib2:... \
              perf.data
```

And, worst-case, if you also use split debug files in non-standard locations, do this:

```
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              --extraLibPaths /path/to/lib1:/path/to/lib2:... \
              --debugPaths /path/to/debug1:/path/to/debug2:... \
              perf.data
```

## Known Issues

If anything breaks in the above and the output is less usable then `perf report`, please [report an issue on GitHub](https://github.com/KDAB/hotspot/issues).
That said, there are some known issues that people may trip over:

### Broken Backtraces

Unwinding the stack to produce a backtrace is a dark art and can go wrong in many ways.
Hotspot relies on `perfparser` (see below), which in turn relies on `libdw` from elfutils
to unwind the stack. This works quite well most of the time, but still can go wrong. Most
notably, unwinding will fail when:

- an ELF file (i.e. executable or library) referenced by the `perf.data` file is missing
  - to fix this, try to use one of the following CLI arguments to let hotspot know where to look for the ELF files:
    - `--debugPaths <paths>`: Use this when you have split debug files in non-standard locations
    - `--extraLibPaths <paths>`: Use this when you have moved libraries to some other location since recording
    - `--appPath <paths>`: This is kind of a combination of the above two fields. The path is traversed recursively, looking for debug files and libraries.
    - `--sysroot <path>`: Use this when you try to inspect a data file recorded on an embedded platform
- an ELF file is missing debug information
  - to fix this, install the debug package from your distro
  - or compile the code in "release with debug" mode, i.e. ensure your compiler is invoked with something like `-O2 -g`. You will have to repeat the `perf record` step
  - potentially both of the above is not an option for you, e.g. when the library is closed source and supplied by a thirdparty vendor. If that is the case,
   you may be lucky and the library contains frame pointers. If so, then try to build elfutils from current git master (you want commit a55df2c1, which should be part of 0.170).
   This version of elfutils will try to fallback to the frame pointer for unwinding, when the debug information is missing.
- you are using clang to compile your code
  - sadly, it seems like clang is producing garbage DWARF information that elfutils fails to interpret, see also: https://github.com/KDAB/hotspot/issues/51
  - thankfully, this should only affect inline frames and file/line information. nevertheless, this is a big limitation and significantly decreases the usability of hotspot (and perf, for that matter)
- your call stacks are too deep
  - by default, `perf record` only copies a part of the stack to the data file. This can lead to issues with very deep call stacks, which will be cut off at some point.
    This issue will break the top-down call trees in hotspot, as visualized in the Top-Down view or the Flame Graph. To fix this, you can try to increase the stack dump size, i.e.:

        perf record --call-graph dwarf,32768

    Note that this can dramatically increase the size of the `perf.data` files - use it with care. Also have a look at `man perf record`.

### Missing Features

Compared to `perf report`, hotspot misses a lot of features. Some of these are planned to be resolved
in the future. Others may potentially never get implemented. But be aware that the following features
are _not_ available in hotspot currently:

- tracepoints: we only analyze and show samples. This means that it is currently impossible to do off-CPU profiling with hotspot.
- annotate: the caller/callee view shows cost attributed to individual source lines. But a proper annotation view like `perf annotate`, esp. on the instruction level, is currently missing.
- the columns in the tables are currently hardcoded, while potentially a user may want to change this to show e.g. cost per-process or thread and so forth
- many of the more advanced features, such as `--itrace`, `--mem-mode`, `--branch-stack` and `--branch-history`, are unsupported

## Qt Creator

This project leverages the excellent `perfparser` utility created by The Qt Company
for their Qt Creator IDE. If you are already using Qt Creator, consider leveraging
its integrated [CPU Usage Analyzer](http://doc.qt.io/qtcreator/creator-cpu-usage-analyzer.html).
