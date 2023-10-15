check_submodule_exists(perfparser perfparser/app/perfdata.h)

include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${LIBELF_INCLUDE_DIRS} ${LIBDW_INCLUDE_DIR}/elfutils ${LIBDWARF_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${LIBDW_LIBRARIES} ${LIBELF_LIBRARIES})
check_symbol_exists(dwfl_get_debuginfod_client "libdwfl.h" HAVE_DWFL_GET_DEBUGINFOD_CLIENT_SYMBOL)
set(CMAKE_REQUIRED_LIBRARIES ${LIBDEBUGINFOD_LIBRARIES})
check_symbol_exists(debuginfod_set_user_data "debuginfod.h" HAVE_DEBUGINFOD_SET_USER_DATA)



include_directories(
    ${LIBELF_INCLUDE_DIRS}
    ${LIBDW_INCLUDE_DIR}/elfutils
    ${LIBDWARF_INCLUDE_DIRS}
    perfparser/app
    ${CMAKE_CURRENT_BINARY_DIR}/perfparser/app
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
    Qt::Core
    Qt::Network
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
)

if (Zstd_FOUND)
    target_include_directories(libhotspot-perfparser PUBLIC ${Zstd_INCLUDE_DIR})
    target_link_libraries(libhotspot-perfparser PUBLIC ${Zstd_LIBRARY})
    set(HAVE_ZSTD 1)
endif()

if (HAVE_DWFL_GET_DEBUGINFOD_CLIENT_SYMBOL AND HAVE_DEBUGINFOD_SET_USER_DATA)
    target_link_libraries(libhotspot-perfparser PRIVATE ${LIBDEBUGINFOD_LIBRARIES})
    set(HAVE_DWFL_GET_DEBUGINFOD_CLIENT 1)
endif()

add_feature_info(debuginfod HAVE_DWFL_GET_DEBUGINFOD_CLIENT "libdwfl and libdebuginfod are useful for on-demand fetching of debug symbols")

configure_file(perfparser/app/config-perfparser.h.in perfparser/app/config-perfparser.h)

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
        Qt::Test
    TEST_NAME
        tst_elfmap
)

set_target_properties(tst_elfmap
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

ecm_add_test(
    perfparser/tests/auto/addresscache/tst_addresscache.cpp
    LINK_LIBRARIES
        libhotspot-perfparser
        Qt::Test
    TEST_NAME
        tst_addresscache
)

set_target_properties(tst_addresscache
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

ecm_add_test(
    perfparser/tests/auto/perfdata/tst_perfdata.cpp
    perfparser/tests/auto/shared/perfparsertestclient.cpp
    perfparser/tests/auto/perfdata/perfdata.qrc
    LINK_LIBRARIES
        Qt::Test
        libhotspot-perfparser
    TEST_NAME
        tst_perfdata
)

set_target_properties(tst_perfdata
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
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
    Qt::Core
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
