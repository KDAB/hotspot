include_directories(../../src)

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

ecm_add_test(
    tst_disassemblyoutput.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Test
        models
        PrefixTickLabels
    TEST_NAME
        tst_disassemblyoutput
)

if (KGRAPHVIEWER_FOUND)
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
endif (KGRAPHVIEWER_FOUND)
