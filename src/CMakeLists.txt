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
    resultsdisassemblypage.cpp
    resultsutil.cpp
    costheaderview.cpp
    timelinewidget.cpp
    dockwidgetsetup.cpp
    settingsdialog.cpp
    multiconfigwidget.cpp
    perfoutputwidget.cpp
    perfoutputwidgettext.cpp
    perfoutputwidgetkonsole.cpp
    costcontextmenu.cpp

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
    resultsdisassemblypage.ui
    timelinewidget.ui
    settingsdialog.ui
    flamegraphsettings.ui
    debuginfoddialog.ui

    # resources:
    resources.qrc
)

ecm_add_app_icon(hotspot_SRCS ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/128-hotspot_app_icon.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/512-hotspot_app_icon.png
)

if (QCustomPlot_FOUND)
    set(hotspot_SRCS ${hotspot_SRCS} frequencypage.cpp)
endif()

add_executable(hotspot
    ${hotspot_SRCS}
)

target_link_libraries(hotspot
    Qt5::Widgets
    Qt5::Svg
    KF5::ThreadWeaver
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
    KF5::Notifications
    KF5::IconThemes
    KF5::Parts
    models
    PrefixTickLabels
    KDAB::kddockwidgets
)

if (KF5Archive_FOUND)
    target_link_libraries(hotspot
        KF5::Archive)
endif(KF5Archive_FOUND)

if (QCustomPlot_FOUND)
    target_link_libraries(hotspot
        QCustomPlot::QCustomPlot)
endif(QCustomPlot_FOUND)

set_target_properties(hotspot
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}"
)

install(TARGETS hotspot RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
install(FILES hotspot.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR})

if (KF5Auth_FOUND)
    target_link_libraries(hotspot KF5::AuthCore)

    add_executable(hotspot-auth-helper authhelper.cpp)
    target_link_libraries(hotspot-auth-helper KF5::AuthCore)

    option(INSTALL_KAUTH_HELPER "Install the KAuth helper, defaults to ON but can be disabled when you want to install to a custom prefix" ON)
    if (INSTALL_KAUTH_HELPER)
        install(TARGETS hotspot-auth-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})

        kauth_install_helper_files(hotspot-auth-helper com.kdab.hotspot.perf root)
        kauth_install_actions(com.kdab.hotspot.perf hotspot-perf-authhelper.actions)
    endif ()
endif (KF5Auth_FOUND)
