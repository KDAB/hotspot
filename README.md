<img width="100%" src="src/images/hotspot-logo.png">

# Hotspot - the Linux perf GUI for performance analysis

This project is a [KDAB](http://www.kdab.com) R&D effort to create a standalone
GUI for performance data. As the first goal, we want to provide a UI like
KCachegrind around Linux perf. Looking ahead, we intend to support various other
performance data formats under this umbrella.

## Table of Contents

<!--
    TOC generated with https://github.com/jonschlinkert/markdown-toc
    To update, run `markdown-toc -i README.md`
-->

<!-- toc -->

- [Screenshots](#screenshots)
  - [Visualize Data](#visualize-data)
  - [Time Line](#time-line)
  - [Record Data](#record-data)
- [Getting Hotspot](#getting-hotspot)
  - [ArchLinux](#archlinux)
  - [Debian / Ubuntu](#debian--ubuntu)
  - [Gentoo](#gentoo)
  - [Fedora](#fedora)
  - [For any Linux distro: AppImage](#for-any-linux-distro-appimage)
- [Building Hotspot](#building-hotspot)
- [Using](#using)
  - [Off-CPU Profiling](#off-cpu-profiling)
  - [Embedded Systems](#embedded-systems)
  - [Import Export](#import-export)
  - [tracepoints](#tracepoints)
  - [Disassembler](#disassembler)
- [Known Issues](#known-issues)
  - [Broken Backtraces](#broken-backtraces)
  - [debuginfod](#debuginfod)
  - [Missing Features](#missing-features)
  - [Recording with perf without super user rights](#recording-with-perf-without-super-user-rights)
  - [Export File Format](#export-file-format)
- [Qt Creator](#qt-creator)
- [License](#license)

<!-- tocstop -->

## Screenshots

Here are some screenshots showing the most important features of hotspot in action:

### Visualize Data

The main feature of hotspot is visualizing a `perf.data` file graphically.

![hotspot summary page](screenshots/summary.png?raw=true "hotspot summary page")

![hotspot FlameGraph page](screenshots/flamegraph.png?raw=true "hotspot FlameGraph page")

![hotspot off-CPU analysis](screenshots/off-cpu.png?raw=true "hotspot off-CPU analysis")

![hotspot caller-callee page](screenshots/caller-callee.png?raw=true "hotspot caller-callee page")

![hotspot bottom-up page](screenshots/bottom-up.png?raw=true "hotspot bottom-up page")

![hotspot top-down page](screenshots/top-down.png?raw=true "hotspot top-down page")

![hotspot dockwidget layouts](screenshots/dockwidgets.png?raw=true
"hotspot with custom dockwidget layout and disassembly view")

### Time Line

The time line allows filtering the results by time, process or thread. The data views will update accordingly.

![hotspot timeline filtering by time](screenshots/timeline-filter-time.png?raw=true
"hotspot timeline filtering by time")

![hotspot timeline filtering by thread or process](screenshots/timeline-filter-thread.png?raw=true
"hotspot timeline filtering by thread or process")

![hotspot timeline filtering applied to FlameGraph](screenshots/timeline-flamegraph.png?raw=true
"hotspot timeline filtering also applies to the data views on top, like e.g. the FlameGraph.
You can also zoom in on the timeline and inspect individual sample data.")

### Record Data

You can also launch `perf` from hotspot, to profile a newly started application
or to attach to already running process(es). Do take the
[caveats below](#recording-with-perf-without-super-user-rights) into account though.

![hotspot launch application](screenshots/record-launch.png?raw=true
"hotspot can launch a new application and profile it with perf from the record page.")

![hotspot attach to process](screenshots/record-attach.png?raw=true
"hotspot also allows runtime-attaching of perf to existing applications to profile them.")

## Getting Hotspot

*Note: Hotspot is not yet packaged on all Linux distributions. In such cases, or when you want to use the
latest version, please use the AppImage which will work on any recent Linux distro just fine.*

### ArchLinux

hotspot is available in AUR (https://aur.archlinux.org/packages/hotspot).

### Debian / Ubuntu

hotspot is available in Debian (https://packages.debian.org/hotspot) and Ubuntu
(https://packages.ubuntu.com/hotspot).

### Gentoo

hotspot ebuilds are available from our overlay (https://github.com/KDAB/kdab-overlay).

### Fedora

hotspot is available in Fedora (https://packages.fedoraproject.org/pkgs/hotspot/hotspot/).

### For any Linux distro: AppImage

Head over to our [list of AppImage build jobs][appimagejobs].
When you click on a job, you'll see a page with an "Artifacts" section that contains an "appimage" binary
you can then download. Unzip the AppImage file and make it executable, then run it.

[appimagejobs]: https://github.com/KDAB/hotspot/actions/workflows/build-appimage.yml?query=branch%3Amaster

Please use the latest build to get the most recent version. If it doesn't work, please report a
bug and test the latest stable version.

*Note: Your system libraries or preferences are not altered. In case you'd like to remove Hotspot again,
simply delete the downloaded file. Learn more about AppImage [here](http://appimage.org/).*

To find out how to debug the Appimage, see [HACKING](HACKING.md#debugging-the-appimage).

## Building Hotspot

Building hotspot from source gives you the latest and greatest, but you'll have to make sure all its
dependencies are available. Most users should probably [install hotspot](#getting-hotspot) from the distro
package manager or as an [AppImage](#for-any-linux-distro-appimage).

For everyone that wants to contribute to Hotspot or use the newest version without the Appimage detailed
notes are found at [HACKING](HACKING.md#table-of-contents).

## Using

### General

First of all, record some data with `perf`. To get backtraces, you will need to enable the dwarf callgraph
mode:

```bash
perf record --call-graph dwarf <your application>
...
[ perf record: Woken up 58 times to write data ]
[ perf record: Captured and wrote 14.874 MB perf.data (1865 samples) ]
```

Now, if you have hotspot available on the same machine, all you need to do is launch it.
It will automatically open the `perf.data` file in the current directory (similar to `perf report`).

Alternatively, you can specify the path to the data file on the console:

```bash
hotspot /path/to/perf.data
```

### Command Line options

Depending on your needs you may want to pass additional command line options to hotspot.
This allows to one-time set configuration options that are found in the GUI under "Settings"
and also allows to convert Linux perf data files into the smaller and portable perfdata format
(see [Import / Export](#import-export) for details on that).
All command line options are shown with `--help`:

```text
Usage: hotspot [options] [files...]
Linux perf GUI for performance analysis.

Options:
  -h, --help               Displays help on commandline options.
  --help-all               Displays help including Qt specific options.
  -v, --version            Displays version information.
  --sysroot <path>         Path to sysroot which is used to find libraries.
  --kallsyms <path>        Path to kallsyms file which is used to resolve
                           kernel symbols.
  --debugPaths <paths>     Colon separated list of paths that contain debug
                           information. These paths are relative to the
                           executable and not to the current working directory.
  --extraLibPaths <paths>  Colon separated list of extra paths to find
                           libraries.
  --appPath <path>         Path to folder containing the application executable
                           and libraries.
  --arch <path>            Architecture to use for unwinding.
  --exportTo <path>        Path to .perfparser output file to which the input
                           data should be exported. A single input file has to
                           be given too.

Arguments:
  files                    Optional input files to open on startup, i.e.
                           perf.data files.
```

### Off-CPU Profiling

Hotspot supports a very powerful way of doing wait-time analysis, or off-CPU profiling.
This analysis is based on kernel tracepoints in the linux scheduler. By recording that
data, we can find the time delta during which a thread was not running on the CPU, but
instead was off-CPU. There can be multiple reasons for that, all of which can be found
using this technique:

- synchronous I/O, e.g. via `read()` or `write()`
- page faults, e.g. when accessing `mmap()`'ed file data
- calls to `nanosleep()` or `yield()`
- lock contention via `futex()` etc.
- preemption
- and probably many more

By leveraging kernel trace points in the scheduler, the overhead is pretty manageable
and we only pay a price, when the process is actually getting switched out. Most notably
we do not pay a price when e.g. a mutex lock operation can be handled directly in
user-space.

To do off-CPU analysis with hotspot, you need to record the data with a very specific
command:

```bash
perf record \
    -e cycles \                             # on-CPU profiling
    -e sched:sched_switch --switch-events \ # off-CPU profiling
    --sample-cpu \                          # track on which core code is executed
    -m 8M \                                 # reduce chance of event loss
    --aio -z \                              # reduce disk-I/O overhead and data size
    --call-graph dwarf \                    # we definitely want backtraces
    <your application>
```

Alternatively, you can use the off-CPU check box in hotspot's integrated record page.

During the analysis, you can then switch between the "cycles" cost view for on-CPU data
to the "off-CPU time" cost view for wait-time analysis. Often, you will want to change
between both, e.g. to find places in your code which may require further parallelization
(see also [Amdahl's law](https://en.wikipedia.org/wiki/Amdahl%27s_law)).

The "sched:sched_switch" cost will also be shown to you. But in my opinion that is less
useful, as it only indicates the number of scheduler switches. The length of the time
inbetween is often way more interesting to me - and that's what is shown to you in the
"off-CPU time" metric.

### Embedded Systems

If you are recording on an embedded system, you will want to analyze the data on your
development machine with hotspot. To do so, make sure your sysroot contains the debug
information required for unwinding (see below). Then record the data on your embedded
system:

```bash
embedded$ perf record --call-graph dwarf <your application>
...
[ perf record: Woken up 58 times to write data ]
[ perf record: Captured and wrote 14.874 MB perf.data (1865 samples) ]
embedded$ cp /proc/kallsyms /tmp/kallsyms # make pseudo-file a real file
```

It's OK if your embedded machine is using a different platform than your host. On your
host, do the following steps then to analyze the data:

```bash
host$ scp embedded:perf.data embedded:/tmp/kallsyms .
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms \
              perf.data
```

If you manually deployed an application from a path outside your sysroot, do this instead:

```bash
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              perf.data
```

If your application is also using libraries outside your sysroot and the appPath, do this:

```bash
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              --extraLibPaths /path/to/lib1:/path/to/lib2:... \
              perf.data
```

And, worst-case, if you also use split debug files in non-standard locations, do this:

```bash
host$ hotspot --sysroot /path/to/sysroot --kallsyms kallsyms --appPath /path/to/app \
              --extraLibPaths /path/to/lib1:/path/to/lib2:... \
              --debugPaths /path/to/debug1:/path/to/debug2:... \
              perf.data
```

### Import Export

The `perf.data` file format is not self-contained. To analyze it, you need access
to the executables and libraries of the profiled process, together with debug symbols.
This makes it unwieldy to share such files across machines, e.g. to get the help from
a colleague to investigate a performance issue, or for bug reporting purposes.

Hotspot allows you to export the analyzed data, which is then fully self-contained.
This feature is accessible via the "File > Save As" menu action. The data is then
saved in a self-contained `*.perfparser` file. To import the data into hotspot again,
just open that file directly in place of the original `perf.data` file.

**Note:** The file format is _not_ yet stable. Meaning data exported by one version
of hotspot can only be read back in by the same version. This problem will be
resolved in the future, as time permits.

### tracepoints

hotspot currently only shows the name of the tracepoints in the timeline.

![hotspot tracepoints](screenshots/tracepoints.png?raw=true "hotspot tracepoints")

### Disassembler

![hotspot disassembler page](screenshots/disassembler.png?raw=true "hotspot disassembler page")

Hotspot includes an disassembler, which can show you the cost per instruction.
The disassembler uses colors to indicate which assembly lines correspond to
which source code line.
For easier navigation, you can simply click on a line and the other view will jump to it.
You can follow function calls with a double click.
In the sourcecode view you can press ctrl+f or press the search icon to open a search window.

## Known Issues

If anything breaks in the above and the output is less usable than `perf report`, please
[report an issue on GitHub](https://github.com/KDAB/hotspot/issues).
That said, there are some known issues that people may trip over:

### Broken Backtraces

Unwinding the stack to produce a backtrace is a dark art and can go wrong in many ways.
Hotspot relies on `perfparser` (see below), which in turn relies on `libdw` from elfutils
to unwind the stack. This works quite well most of the time, but still can go wrong. Most
notably, unwinding will fail when:

- an ELF file (i.e. executable or library) referenced by the `perf.data` file is missing
  - to fix this, try to use one of the following CLI arguments to let hotspot know where to look for the
    ELF files:
    - `--debugPaths <paths>`: Use this when you have split debug files in non-standard locations
    - `--extraLibPaths <paths>`: Use this when you have moved libraries to some other location since recording
    - `--appPath <paths>`: This is kind of a combination of the above two fields. The path is traversed
     recursively, looking for debug files and libraries.
    - `--sysroot <path>`: Use this when you try to inspect a data file recorded on an embedded platform
- an ELF file is missing debug information
  - to fix this, install the debug package from your distro
  - or compile the code in "release with debug" mode, i.e. ensure your compiler is invoked with something
    like `-O2 -g`. You will have to repeat the `perf record` step
  - potentially both of the above is not an option for you, e.g. when the library is closed source and
    supplied by a thirdparty vendor. If that is the case,
    you may be lucky and the library contains frame pointers. If so, then try to build elfutils from current
    git master (you want commit a55df2c1, which should be part of 0.170).
    This version of elfutils will try to fallback to the frame pointer for unwinding, when the debug
    information is missing.
- your call stacks are too deep
  - by default, `perf record` only copies a part of the stack to the data file. This can lead to issues with
    very deep call stacks, which will be cut off at some point. This issue will break the top-down call trees
    in hotspot, as visualized in the Top-Down view or the Flame Graph. To fix this, you can try to increase
    the stack dump size, i.e.:

        perf record --call-graph dwarf,32768

    Note that this can dramatically increase the size of the `perf.data` files - use it with care. Also have a
    look at `man perf record`.
  - For some scenarios, recursive function calls simply fail to be unwound. See also
    https://github.com/KDAB/hotspot/issues/93

### debuginfod

hotspot supports downloading debug symbols via [debuginfod](https://sourceware.org/elfutils/Debuginfod.html).
This can be enabled by either adding download urls in the settings or launching hotspot with `DEBUGINFOD_URLS`
defined in the environment.

### Missing Features

Compared to `perf report`, hotspot misses a lot of features. Some of these are planned to be resolved
in the future. Others may potentially never get implemented. But be aware that the following features
are _not_ available in hotspot currently:

- the columns in the tables are currently hardcoded, while potentially a user may want to change this to show
  e.g. cost per-process or thread and so forth. Basic group-by functionality was added, but a more flexible
  column arrangement a la WPA is not planned for now.
- many of the more advanced features, such as `--itrace`, `--mem-mode`, `--branch-stack` and
  `--branch-history`, are unsupported

### Recording with perf without super user rights

It is **not** a good idea to launch hotspot with `sudo` or as `root` user. See e.g.
[Editing Files As Root](https://blog.martin-graesslin.com/blog/2017/02/editing-files-as-root/)
for an article on that matter. [Issue #83](https://github.com/KDAB/hotspot/issues/83) is
also relevant in this contact.

But without superuser rights, you may see error messages such as the following
when using hotspot's record feature:

```text
You may not have permission to collect stats.
Consider tweaking /proc/sys/kernel/perf_event_paranoid:
  -1 - Not paranoid at all
   0 - Disallow raw tracepoint access for unpriv
   1 - Disallow cpu events for unpriv
   2 - Disallow kernel profiling for unpriv
```

To workaround this limitation, hotspot can temporarily elevate the perf privileges.
This is achieved by applying
[these steps](https://superuser.com/questions/980632/run-perf-without-root-right),
bundled into [a script](scripts/elevate_perf_privileges.sh) that is run via `pkexec`, `kdesudo` or `kdesu`.
The resulting elevated privileges are also required for kernel tracing in general and Off-CPU profiling in
particular.

### Export File Format

The current data export is limited to a format that can only be read back by hotspot of the same
version. This makes interop with other visualization tools quasi impossible. This is known and
will get improved in the future. Most notably support for export to web viewers such as
[perfetto](https://perfetto.dev/) or the [Mozilla profiler](https://profiler.firefox.com/) is
planned but not yet implemented. Patches welcome!

## Qt Creator

This project leverages the excellent `perfparser` utility created by The Qt Company
for their Qt Creator IDE. If you are already using Qt Creator, consider leveraging
its integrated [CPU Usage Analyzer](http://doc.qt.io/qtcreator/creator-cpu-usage-analyzer.html).

## License

Hotspot is the GPL v2+. See [LICENSE.GPL.txt](LICENSE.GPL.txt) for more information, or contact
info@kdab.com if any conditions of this licensing are not clear to you.
