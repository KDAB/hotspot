# enable more pedantic compiler warnings
include(ECMInstallIcons)
include(ECMAddAppIcon)

include(KDEFrameworkCompilerSettings)
# but allow signal/slot keywords
remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(models)

ecm_install_icons(ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/16-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/22-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/32-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/48-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/64-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/128-apps-hotspot.png
    DESTINATION ${ICON_INSTALL_DIR} THEME hicolor)

set(hotspot_SRCS
    main.cpp
    util.cpp

    parsers/perf/perfparser.cpp
    perfrecord.cpp

    mainwindow.cpp
    flamegraph.cpp
    aboutdialog.cpp

    # ui files:
    mainwindow.ui
    aboutdialog.ui

    # resources:
    resources.qrc
)

ecm_add_app_icon(hotspot_SRCS ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/128-hotspot_app_icon.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/512-hotspot_app_icon.png
)

add_executable(hotspot
    ${hotspot_SRCS}
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
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
