include_directories(../../src)
include_directories(../../src/models)
include_directories(../../src/parsers/perf)

ecm_add_test(
    ../../src/initiallystoppedprocess.cpp
    ../../src/perfcontrolfifowrapper.cpp
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
    KF5::KIOCore
    KF5::Parts
    TEST_NAME
    tst_perfparser
)

if(KF5Archive_FOUND)
    target_link_libraries(tst_perfparser KF5::Archive)
endif()

set_target_properties(tst_perfparser PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

add_executable(
    dump_perf_data
    ../../src/models/data.cpp
    ../../src/parsers/perf/perfparser.cpp
    ../../src/settings.cpp
    ../../src/util.cpp
    dump_perf_data.cpp
)
target_link_libraries(
    dump_perf_data
    Qt5::Core
    Qt5::Gui
    Qt5::Test
    KF5::ThreadWeaver
    KF5::KIOCore
    KF5::Parts
)

if(KF5Archive_FOUND)
    target_link_libraries(dump_perf_data KF5::Archive)
endif()

set_target_properties(dump_perf_data PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

include_directories(${LIBELF_INCLUDE_DIRS} ${LIBDW_INCLUDE_DIR}/elfutils ${LIBDWARF_INCLUDE_DIRS})

add_executable(
    elfwalk
    elfwalk.cpp
)

target_link_libraries(
    elfwalk LINK_PRIVATE ${LIBDW_LIBRARIES} ${LIBELF_LIBRARIES}
)
