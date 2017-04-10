include_directories(../../src)
include_directories(../../src/models)
include_directories(../../src/parsers/perf)

ecm_add_test(
    ../../src/perfrecord.cpp
    ../../src/util.cpp
    ../../src/models/data.cpp
    ../../src/parsers/perf/perfparser.cpp
    tst_perfparser.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Test
        KF5::ThreadWeaver
    TEST_NAME
        tst_perfparser
)

set_target_properties(tst_perfparser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)
