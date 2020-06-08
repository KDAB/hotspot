# enable more pedantic compiler warnings
include(ECMInstallIcons)
include(ECMAddAppIcon)

include(KDEFrameworkCompilerSettings)
# but allow signal/slot keywords
remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

option(APPIMAGE_BUILD "configure build for bundling in an appimage" OFF)

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

    parsers/perf/perfparser.cpp
    perfrecord.cpp

    mainwindow.cpp
    flamegraph.cpp
    aboutdialog.cpp
    startpage.cpp
    recordpage.cpp
    resultspage.cpp
    resultssummarypage.cpp
    resultstopdownpage.cpp
    resultsbottomuppage.cpp
    resultsflamegraphpage.cpp
    resultscallercalleepage.cpp
    resultsutil.cpp
    costheaderview.cpp

    # ui files:
    mainwindow.ui
    aboutdialog.ui
    startpage.ui
    recordpage.ui
    resultspage.ui
    resultssummarypage.ui
    resultstopdownpage.ui
    resultsbottomuppage.ui
    resultsflamegraphpage.ui
    resultscallercalleepage.ui

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
    Qt5::Svg
    KF5::ThreadWeaver
    KF5::I18n
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::ItemViews
    KF5::ItemModels
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::KIONTLM
    KF5::Solid
    KF5::WindowSystem
    models
    PrefixTickLabels
)

set_target_properties(hotspot
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
