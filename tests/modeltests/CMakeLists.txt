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
