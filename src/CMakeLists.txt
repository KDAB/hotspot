include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(hotspot
    main.cpp
    util.cpp

    parsers/perf/perfparser.cpp

    models/costmodel.cpp
    models/topproxy.cpp

    mainwindow.cpp
    flamegraph.cpp

    # ui files:
    mainwindow.ui

    # resources:
    resources.qrc
)

target_link_libraries(hotspot
    Qt5::Widgets
    KF5::ThreadWeaver
    KF5::I18n
    KF5::ConfigWidgets
    KF5::ItemViews
    KF5::ItemModels
)

set_target_properties(hotspot
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${BIN_INSTALL_DIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${BIN_INSTALL_DIR})
