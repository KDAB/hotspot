include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${LIBELF_INCLUDE_DIRS} ${LIBDW_INCLUDE_DIR}/elfutils ${LIBDWARF_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${LIBDW_LIBRARIES} ${LIBELF_LIBRARIES})
check_symbol_exists(dwfl_get_debuginfod_client "libdwfl.h" HAVE_DWFL_GET_DEBUGINFOD_CLIENT)

include_directories(
    ${LIBELF_INCLUDE_DIRS}
    ${LIBDW_INCLUDE_DIR}/elfutils
    ${LIBDWARF_INCLUDE_DIRS}
    perfparser/app
)

add_library(libhotspot-perfparser STATIC
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
    perfparser/app/demangler.cpp
)

target_link_libraries(libhotspot-perfparser
PUBLIC
    Qt5::Core
    Qt5::Network
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
)

if (Zstd_FOUND)
    target_include_directories(libhotspot-perfparser PUBLIC ${Zstd_INCLUDE_DIR})
    target_link_libraries(libhotspot-perfparser PUBLIC ${Zstd_LIBRARY})
    target_compile_definitions(libhotspot-perfparser PUBLIC HAVE_ZSTD=1)
endif()

if (HAVE_DWFL_GET_DEBUGINFOD_CLIENT)
    target_link_libraries(libhotspot-perfparser PRIVATE ${LIBDEBUGINFOD_LIBRARIES})
    target_compile_definitions(libhotspot-perfparser PRIVATE HAVE_DWFL_GET_DEBUGINFOD_CLIENT=1)
endif()

add_executable(hotspot-perfparser
    perfparser/app/main.cpp
)

target_link_libraries(hotspot-perfparser
PRIVATE
    libhotspot-perfparser
)

set_target_properties(hotspot-perfparser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_LIBEXECDIR}"
)

install(TARGETS hotspot-perfparser RUNTIME DESTINATION ${KDE_INSTALL_LIBEXECDIR})

ecm_add_test(
    perfparser/tests/auto/elfmap/tst_elfmap.cpp
    LINK_LIBRARIES
        libhotspot-perfparser
        Qt5::Test
    TEST_NAME
        tst_elfmap
)

ecm_add_test(
    perfparser/tests/auto/addresscache/tst_addresscache.cpp
    LINK_LIBRARIES
        libhotspot-perfparser
        Qt5::Test
    TEST_NAME
        tst_addresscache
)

ecm_add_test(
    perfparser/tests/auto/perfdata/tst_perfdata.cpp
    perfparser/tests/auto/shared/perfparsertestclient.cpp
    perfparser/tests/auto/perfdata/perfdata.qrc
    LINK_LIBRARIES
        Qt5::Test
        libhotspot-perfparser
    TEST_NAME
        tst_perfdata
)

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

add_custom_command(TARGET hotspot-perfparser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${PROJECT_BINARY_DIR}/${KDE_INSTALL_LIBEXECDIR}/hotspot-perfparser"
        "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}/perfparser"
    BYPRODUCTS "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}/perfparser"
)
