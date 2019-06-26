include_directories(../../src)
include_directories(../../src/models)
include_directories(../../src/parsers/perf)

ecm_add_test(
    ../../src/perfrecord.cpp
    ../../src/settings.cpp
    ../../src/util.cpp
    ../../src/models/data.cpp
    ../../src/parsers/perf/perfparser.cpp
    tst_perfparser.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Test
        KF5::ThreadWeaver
        KF5::CoreAddons
        KF5::WindowSystem
    TEST_NAME
        tst_perfparser
)

set_target_properties(tst_perfparser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

add_executable(dump_perf_data
    dump_perf_data.cpp
    ../../src/settings.cpp
    ../../src/util.cpp
    ../../src/models/data.cpp
    ../../src/parsers/perf/perfparser.cpp
)
target_link_libraries(dump_perf_data
    Qt5::Core
    Qt5::Test
    KF5::ThreadWeaver
)
set_target_properties(dump_perf_data
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)
