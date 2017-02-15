include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(models)

add_executable(hotspot
    main.cpp
    util.cpp

    parsers/perf/perfparser.cpp

    mainwindow.cpp
    flamegraph.cpp
    aboutdialog.cpp

    # ui files:
    mainwindow.ui
    aboutdialog.ui

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
    models
)

set_target_properties(hotspot
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${BIN_INSTALL_DIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${BIN_INSTALL_DIR})
