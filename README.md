# Hotspot

The Qt GUI for performance analysis.

This project is a [KDAB](http://www.kdab.com) R&D effort to create a standalone
GUI for performance data. As the first goal, we want to provide a UI like
KCachegrind around Linux perf. Looking ahead, we intend to support various other
performance data formats under this umbrella.

## Dependencies

As of now, you will need the following dependencies to build this project:

- CMake 3.1.0 or higher
- any C++14 enabled compiler
- Qt 5.6.0 or higher
- libelf
- libelfutils

## Building and running

```
git clone git@github.com:KDAB/hotspot.git
mkdir build-hotspot
cd build-hotspot
cmake ../hotspot
make
./bin/hotspot
```

If you need help building this project for your platform, [contact us for help](https://www.kdab.com/about/contact/).

## Known Issues

### Broken Backtraces

Unwinding the stack to produce a backtrace is a dark art and can go wrong in many ways.
Hotspot relies on `perfparser` (see below), which in turn relies on `libdw` from elfutils
to unwind the stack. This works quite well most of the time, but still can go wrong. Most
notably, unwinding will fail when:

- an ELF file (i.e. executable or library) referenced by the `perf.data` file is missing
-- to fix this, try to use one of the following CLI arguments to let hotspot know where to look for the ELF files:
--- `--debugPaths <paths>`: Use this when you have split debug files in non-standard locations
--- `--extraLibPaths <paths>`: Use this when you have moved libraries to some other location since recording
--- `--appPath <paths>`: This is kind of a combination of the above two fields. The path is traversed recursively, looking for debug files and libraries.
--- `--sysroot <path>`: Use this when you try to inspect a data file recorded on an embedded platform
- an ELF file is missing debug information
-- to fix this, install the debug package from your distro
-- or compile the code in "release with debug" mode, i.e. ensure your compiler is invoked with something like `-O2 -g`. You will have to repeat the `perf record` step
-- potentially both of the above is not an option for you, e.g. when the library is closed source and supplied by a thirdparty vendor. If that is the case,
   you may be lucky and the library contains frame pointers. If so, then try to build elfutils from current git master (you want commit a55df2c1, which should be part of 0.170).
   This version of elfutils will try to fallback to the frame pointer for unwinding, when the debug information is missing.

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
