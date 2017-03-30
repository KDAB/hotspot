include_directories(
    ${LIBELF_INCLUDE_DIR}
    ${LIBDW_INCLUDE_DIR}/elfutils
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
)

target_link_libraries(hotspot-perfparser
    Qt5::Core
    Qt5::Network
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
)

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
    perfparser/tests/auto/kallsyms/tst_kallsyms.cpp
    perfparser/app/perfkallsyms.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Network
        Qt5::Test
        ${LIBDW_LIBRARIES}
        ${LIBELF_LIBRARIES}
    TEST_NAME
        tst_kallsyms
)
