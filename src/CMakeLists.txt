# enable more pedantic compiler warnings
include(ECMInstallIcons)
include(ECMAddAppIcon)

include(KDEFrameworkCompilerSettings)
# but allow signal/slot keywords
remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(models)

ecm_install_icons(
    ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/16-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/22-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/32-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/48-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/64-apps-hotspot.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/128-apps-hotspot.png
    DESTINATION
    ${KDE_INSTALL_ICONDIR}
    THEME
    hicolor
)

set(HOTSPOT_SRCS
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
    initiallystoppedprocess.cpp
    perfcontrolfifowrapper.cpp
    errnoutil.cpp
    recordhost.cpp
    copyabletreeview.cpp
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
    unwindsettingspage.ui
    flamegraphsettingspage.ui
    debuginfodpage.ui
    callgraphwidget.ui
    callgraphsettingspage.ui
    frequencypage.ui
    disassemblysettingspage.ui
    perfsettingspage.ui
    # resources:
    resources.qrc
)

if(KGraphViewerPart_FOUND)
    set(HOTSPOT_SRCS ${HOTSPOT_SRCS} callgraphwidget.cpp callgraphgenerator.cpp)
endif()

ecm_add_app_icon(HOTSPOT_SRCS ICONS ${CMAKE_CURRENT_SOURCE_DIR}/images/icons/128-apps-hotspot.png)

if(QCustomPlot_FOUND)
    set(HOTSPOT_SRCS ${HOTSPOT_SRCS} frequencypage.cpp)
endif()

add_executable(
    hotspot
    ${HOTSPOT_SRCS}
)

target_link_libraries(
    hotspot
    Qt::Widgets
    Qt::Svg
    KF${QT_MAJOR_VERSION}::ThreadWeaver
    KF${QT_MAJOR_VERSION}::ConfigWidgets
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::ItemViews
    KF${QT_MAJOR_VERSION}::ItemModels
    KF${QT_MAJOR_VERSION}::KIOCore
    KF${QT_MAJOR_VERSION}::KIOFileWidgets
    KF${QT_MAJOR_VERSION}::KIOWidgets
    KF${QT_MAJOR_VERSION}::Solid
    KF${QT_MAJOR_VERSION}::WindowSystem
    KF${QT_MAJOR_VERSION}::Notifications
    KF${QT_MAJOR_VERSION}::IconThemes
    KF${QT_MAJOR_VERSION}::Parts
    KF${QT_MAJOR_VERSION}::I18n
    models
    PrefixTickLabels
    KDAB::kddockwidgets
)

if(KFArchive_FOUND)
    target_link_libraries(hotspot KF${QT_MAJOR_VERSION}::Archive)
endif()

if(QCustomPlot_FOUND)
    target_link_libraries(hotspot QCustomPlot::QCustomPlot)
endif()

set_target_properties(hotspot PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_BINDIR}")

install(
    TARGETS hotspot
    RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
)

if(NOT KDE_INSTALL_KNOTIFYRCDIR)
    set(KDE_INSTALL_KNOTIFYRCDIR ${KDE_INSTALL_KNOTIFY5RCDIR})
endif()
install(
    FILES hotspot.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)
