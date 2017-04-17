# enable more pedantic compiler warnings
include(ECMInstallIcons)
include(ECMAddAppIcon)

include(KDEFrameworkCompilerSettings)
# but allow signal/slot keywords
remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(models)

set(hotspot_breeze_ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze/16-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze/22-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze/32-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze/48-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze/64-apps-hotspot.svgz
)

# TODO: this is imo just a workaround, we shouldn't install it as hicolor
ecm_install_icons(ICONS ${hotspot_breeze_ICONS}
    DESTINATION ${ICON_INSTALL_DIR}
    THEME hicolor)

ecm_install_icons(ICONS ${hotspot_breeze_ICONS}
    DESTINATION ${ICON_INSTALL_DIR}
    THEME breeze)

set(hotspot_breeze_dark_ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze-dark/16-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze-dark/22-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze-dark/32-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze-dark/48-apps-hotspot.svgz
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/breeze-dark/64-apps-hotspot.svgz
)

ecm_install_icons(ICONS ${hotspot_breeze_dark_ICONS}
    DESTINATION ${ICON_INSTALL_DIR}
    THEME breeze-dark)

set(hotspot_oxygen_ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/oxygen/256-apps-hotspot.png
)

ecm_install_icons(ICONS ${hotspot_oxygen_ICONS}
    DESTINATION ${ICON_INSTALL_DIR}
    THEME oxygen)

set(hotspot_SRCS
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

ecm_add_app_icon(hotspot_SRCS ICONS
    ${hotspot_breeze_ICONS})

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
