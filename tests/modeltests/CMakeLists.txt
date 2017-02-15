include_directories(../../src)

ecm_add_test(
    tst_models.cpp
    modeltest.cpp
    LINK_LIBRARIES
        Qt5::Core
        Qt5::Test
        models
    TEST_NAME
        tst_models
)
