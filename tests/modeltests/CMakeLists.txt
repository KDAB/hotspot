include_directories(../../src)

add_library(
    fib SHARED
    disassembly/fib.cpp
)

ecm_add_test(
    tst_models.cpp
    LINK_LIBRARIES
    Qt::Core
    Qt::Test
    models
    PrefixTickLabels
    TEST_NAME
    tst_models
)

set_target_properties(tst_models PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

ecm_add_test(
    tst_timelinedelegate.cpp
    LINK_LIBRARIES
    Qt::Core
    Qt::Test
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
    ../../src/settings.cpp
    LINK_LIBRARIES
    Qt::Core
    Qt::Test
    models
    PrefixTickLabels
    TEST_NAME
    tst_disassemblyoutput
)

set_target_properties(
    tst_disassemblyoutput PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

ecm_add_test(
    tst_callgraphgenerator.cpp
    ../../src/parsers/perf/perfparser.cpp
    ../../src/callgraphgenerator.cpp
    ../../src/errnoutil.cpp
    LINK_LIBRARIES
    Qt::Core
    Qt::Test
    KF${QT_MAJOR_VERSION}::KIOCore
    KF${QT_MAJOR_VERSION}::ThreadWeaver
    KF${QT_MAJOR_VERSION}::WindowSystem
    models
    TEST_NAME
    tst_callgraphgenerator
)
if(${KFArchive_FOUND})
    target_link_libraries(tst_callgraphgenerator KF${QT_MAJOR_VERSION}::Archive)
endif()

set_target_properties(
    tst_callgraphgenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

ecm_add_test(
    tst_search.cpp
    LINK_LIBRARIES
    Qt::Test
    TEST_NAME
    tst_search
)
set_target_properties(tst_search PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

ecm_add_test(
    tst_formatting.cpp
    LINK_LIBRARIES
    Qt::Test
    models
    KF${QT_MAJOR_VERSION}::ConfigWidgets
    TEST_NAME
    tst_formatting
)
set_target_properties(tst_formatting PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

ecm_add_test(
    tst_tracepointformat.cpp
    LINK_LIBRARIES
    Qt::Test
    models
    TEST_NAME
    tst_tracepointformat
)
set_target_properties(
    tst_tracepointformat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)
