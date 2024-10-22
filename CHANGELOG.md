# Changelog

## [1.6.0](https://github.com/KDAB/hotspot/compare/v1.5.1...v1.6.0) (2024-10-22)


### Features

* allow handling of compressed perfparser files ([90451a7](https://github.com/KDAB/hotspot/commit/90451a74de3ad58367fa99f21c987177d0fe3aaa))
* allow user to specify perf and objdump via command line ([081812d](https://github.com/KDAB/hotspot/commit/081812dba0470990b3e7d1e5f435459767c8efca)), closes [#556](https://github.com/KDAB/hotspot/issues/556)
* open more perf files ([b0d370f](https://github.com/KDAB/hotspot/commit/b0d370fa1348edbd0b46a90254f3af6e2f50c72a))
* show current unwind settings in settings dialog ([ec5632c](https://github.com/KDAB/hotspot/commit/ec5632c1090ac6e6ea71cf5325fd1b53feda2d49))
* update icons ([d5d9cd4](https://github.com/KDAB/hotspot/commit/d5d9cd48aabf2cc7ef0e36363d3227f215b164f9)), closes [#472](https://github.com/KDAB/hotspot/issues/472)


### Bug Fixes

* also support ' and , as branch visualization characters ([f326ef0](https://github.com/KDAB/hotspot/commit/f326ef0d2f651d45ceff36050039cbccb2eac6c7))
* correct cost attribution of inline frames in disassembly view ([36bedef](https://github.com/KDAB/hotspot/commit/36bedef070d5b846d2be416e96a860220904e14a))
* crash if hotspot is closed while loading a file ([221d60e](https://github.com/KDAB/hotspot/commit/221d60e92f3ca2a5fe0530b1ec48b71f86112984)), closes [#654](https://github.com/KDAB/hotspot/issues/654)
