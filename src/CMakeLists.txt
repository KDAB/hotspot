include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(hotspot
    main.cpp
    util.cpp

    parsers/perf/perfparser.cpp

    models/costmodel.cpp

    mainwindow.cpp

    # ui files:
    mainwindow.ui

    # resources:
    resources.qrc
)

target_link_libraries(hotspot
    Qt5::Widgets
)

set_target_properties(hotspot
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${BIN_INSTALL_DIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${BIN_INSTALL_DIR})
