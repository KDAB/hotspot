include_directories(../../src)

add_library(
    fib SHARED
    disassembly/fib.cpp
)

ecm_add_test(
    tst_models.cpp
    LINK_LIBRARIES
    Qt5::Core
    Qt5::Test
    models
    PrefixTickLabels
    TEST_NAME
    tst_models
)

set_target_properties(tst_models PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

ecm_add_test(
    tst_timelinedelegate.cpp
    LINK_LIBRARIES
    Qt5::Core
    Qt5::Test
    models
    PrefixTickLabels
    TEST_NAME
    tst_timelinedelegate
)

set_target_properties(
    tst_timelinedelegate PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

ecm_add_test(
    tst_disassemblyoutput.cpp
    LINK_LIBRARIES
    Qt5::Core
    Qt5::Test
    models
    PrefixTickLabels
    TEST_NAME
    tst_disassemblyoutput
    ../../src/settings.cpp
)

set_target_properties(
    tst_disassemblyoutput PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

if(KGraphViewerPart_FOUND)
    ecm_add_test(
        tst_callgraphgenerator.cpp
        ../../src/parsers/perf/perfparser.cpp
        ../../src/perfrecord.cpp
        ../../src/callgraphgenerator.cpp
        LINK_LIBRARIES
        Qt5::Core
        Qt5::Test
        KF5::KIOCore
        KF5::ThreadWeaver
        KF5::Archive
        KF5::WindowSystem
        models
        TEST_NAME
        tst_callgraphgenerator
    )

    set_target_properties(
        tst_callgraphgenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
    )
endif()
