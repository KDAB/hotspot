include(perfparser.cmake)
include(PrefixTickLabels.cmake)
add_subdirectory(fmtparser)

set(CMAKE_CSTANDARD 11)
set(CMAKE_C_FLAGS "-Wno-error")
target_compile_options(fmt_parser PRIVATE -Wno-pedantic -Wno-address -Wno-unused-variable -Wno-error)
