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

If you need help building this project for your platform, [contact us for help]
(https://www.kdab.com/about/contact/).

## Qt Creator

This project leverages the excellent `perfparser` utility created by The Qt Company
for their Qt Creator IDE. If you are already using Qt Creator, consider leveraging
its integrated [CPU Usage Analyzer](http://doc.qt.io/qtcreator/creator-cpu-usage-analyzer.html).
