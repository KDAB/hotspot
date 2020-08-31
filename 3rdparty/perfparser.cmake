include_directories(
    ${LIBELF_INCLUDE_DIRS}
    ${LIBDW_INCLUDE_DIR}/elfutils
    ${LIBDWARF_INCLUDE_DIRS}
    perfparser/app
)

add_executable(hotspot-perfparser
    perfparser/app/main.cpp
    perfparser/app/perfattributes.cpp
    perfparser/app/perfheader.cpp
    perfparser/app/perffilesection.cpp
    perfparser/app/perffeatures.cpp
    perfparser/app/perfdata.cpp
    perfparser/app/perfunwind.cpp
    perfparser/app/perfregisterinfo.cpp
    perfparser/app/perfstdin.cpp
    perfparser/app/perfsymboltable.cpp
    perfparser/app/perfelfmap.cpp
    perfparser/app/perfkallsyms.cpp
    perfparser/app/perfaddresscache.cpp
    perfparser/app/perftracingdata.cpp
    perfparser/app/perfdwarfdiecache.cpp
)

target_link_libraries(hotspot-perfparser
LINK_PRIVATE
    Qt5::Core
    Qt5::Network
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
)

if (ZSTD_FOUND)
    target_include_directories(hotspot-perfparser PRIVATE ${ZSTD_INCLUDE_DIR})
    target_link_libraries(hotspot-perfparser PRIVATE ${ZSTD_LIBRARY})
    target_compile_definitions(hotspot-perfparser PRIVATE HAVE_ZSTD=1)
endif()

set(RUSTC_DEMANGLE_INCLUDE_DIR "" CACHE STRING "Path to the folder containing rustc_demangle.h from https://github.com/alexcrichton/rustc-demangle")
set(RUSTC_DEMANGLE_LIBRARY "" CACHE STRING "Path to the librustc_demangle.so library from https://github.com/alexcrichton/rustc-demangle")

if (RUSTC_DEMANGLE_INCLUDE_DIR AND RUSTC_DEMANGLE_LIBRARY)
    target_include_directories(hotspot-perfparser PRIVATE ${RUSTC_DEMANGLE_INCLUDE_DIR})
    target_link_libraries(hotspot-perfparser LINK_PRIVATE ${RUSTC_DEMANGLE_LIBRARY})
    target_compile_definitions(hotspot-perfparser PRIVATE HAVE_RUSTC_DEMANGLE=1)
endif()

set_target_properties(hotspot-perfparser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_LIBEXECDIR}"
)

install(TARGETS hotspot-perfparser RUNTIME DESTINATION ${KDE_INSTALL_LIBEXECDIR})

ecm_add_test(
    perfparser/tests/auto/elfmap/tst_elfmap.cpp
    perfparser/app/perfelfmap.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Network
        Qt5::Test
        ${LIBDW_LIBRARIES}
        ${LIBELF_LIBRARIES}
    TEST_NAME
        tst_elfmap
)

ecm_add_test(
    perfparser/tests/auto/addresscache/tst_addresscache.cpp
    perfparser/app/perfelfmap.cpp
    perfparser/app/perfdwarfdiecache.cpp
    perfparser/app/perfaddresscache.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Network
        Qt5::Test
        ${LIBDW_LIBRARIES}
        ${LIBELF_LIBRARIES}
    TEST_NAME
        tst_addresscache
)

ecm_add_test(
    perfparser/tests/auto/perfdata/tst_perfdata.cpp
    perfparser/tests/auto/shared/perfparsertestclient.cpp
    perfparser/tests/auto/perfdata/perfdata.qrc
    perfparser/app/perfaddresscache.cpp
    perfparser/app/perfattributes.cpp
    perfparser/app/perfdata.cpp
    perfparser/app/perfelfmap.cpp
    perfparser/app/perffeatures.cpp
    perfparser/app/perffilesection.cpp
    perfparser/app/perfheader.cpp
    perfparser/app/perfkallsyms.cpp
    perfparser/app/perfregisterinfo.cpp
    perfparser/app/perfsymboltable.cpp
    perfparser/app/perftracingdata.cpp
    perfparser/app/perfunwind.cpp
    perfparser/app/perfdwarfdiecache.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Network
        Qt5::Test
        ${LIBDW_LIBRARIES}
        ${LIBELF_LIBRARIES}
    TEST_NAME
        tst_perfdata
)

if (ZSTD_FOUND)
    target_include_directories(tst_perfdata PRIVATE ${ZSTD_INCLUDE_DIR})
    target_link_libraries(tst_perfdata ${ZSTD_LIBRARY})
    target_compile_definitions(tst_perfdata PRIVATE HAVE_ZSTD=1)
endif()

include_directories(perfparser/tests/auto/shared)
add_executable(perf2text
    perfparser/tests/manual/perf2text/perf2text.cpp
    perfparser/tests/auto/shared/perfparsertestclient.cpp
)
target_compile_definitions(perf2text
    PRIVATE MANUAL_TEST
)
target_link_libraries(perf2text
    Qt5::Core
)

set_target_properties(perf2text
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

add_custom_target(link_perfparser ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${PROJECT_BINARY_DIR}/${KDE_INSTALL_LIBEXECDIR}/hotspot-perfparser"
        "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}/perfparser"
)
