cmake_minimum_required(VERSION 3.16)
project(
    hotspot
    VERSION 1.4.80
    LANGUAGES CXX C
)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/perfparser/cmake/ ${CMAKE_CURRENT_SOURCE_DIR}/cmake/
                      ${CMAKE_MODULE_PATH}
)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "" FORCE
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(APPIMAGE_BUILD "configure build for bundling in an appimage" OFF)
option(QT6_BUILD "configure build to use Qt6/KF6" OFF)

# Enable the test harness
enable_testing()

# Version setup
set(HOTSPOT_VERSION_STRING "${hotspot_VERSION}")

include(HotspotMacrosInternal)
include(FeatureSummary)

if(QT6_BUILD)
    set(QT_MAJOR_VERSION 6)
    set(QT_MIN_VERSION 6.4)
    set(KDDW_SUFFIX "-qt6")
else()
    set(QT_MAJOR_VERSION 5)
    set(QT_MIN_VERSION 5.15)
    set(KDDW_SUFFIX "")
endif()

find_package(
    Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION}
    COMPONENTS Core
               Widgets
               Network
               Test
               Svg
    REQUIRED
)

find_package(
    QCoro${QT_MAJOR_VERSION}
    COMPONENTS Core
    REQUIRED
)
qcoro_enable_coroutines()

find_package(LibElf REQUIRED)
find_package(ElfUtils REQUIRED)
find_package(ECM 1.0.0 NO_MODULE REQUIRED)
find_package(Threads REQUIRED)
find_package(Zstd)
find_package(objdump)
set_package_properties(
    objdump PROPERTIES
    DESCRIPTION "Utility to generate disassembler"
    PURPOSE "Support for annotating individual instructions"
    URL "https://www.gnu.org/software/binutils/"
    TYPE RUNTIME
)

find_package(LibRustcDemangle)
set_package_properties(
    LibRustcDemangle PROPERTIES
    DESCRIPTION "Demangling for Rust symbols, written in Rust."
    PURPOSE "Demangling of Rust symbols"
    URL "https://github.com/alexcrichton/rustc-demangle"
    TYPE RUNTIME
)

find_package(LibDDemangle)
set_package_properties(
    LibDDemangle PROPERTIES
    DESCRIPTION "Demangling for D symbols, written in D."
    PURPOSE "Demangling of D symbols"
    URL "https://github.com/lievenhey/d_demangler"
    TYPE RUNTIME
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
find_package(
    KF${QT_MAJOR_VERSION} 5.42.0
    COMPONENTS ThreadWeaver
               ConfigWidgets
               CoreAddons
               ItemViews
               ItemModels
               KIO
               Solid
               WindowSystem
               Notifications
               IconThemes
               Parts
               I18n
    REQUIRED
)

find_package(KF${QT_MAJOR_VERSION}Archive 5.85.0 CONFIG)
set(KFArchive_FOUND ${KF${QT_MAJOR_VERSION}Archive_FOUND})
find_package(KF${QT_MAJOR_VERSION}SyntaxHighlighting CONFIG)
set(KFSyntaxHighlighting_FOUND ${KF${QT_MAJOR_VERSION}SyntaxHighlighting_FOUND})

find_package(KDDockWidgets${KDDW_SUFFIX} CONFIG)
set_package_properties(
    KDDockWidgets${KDDW_SUFFIX} PROPERTIES
    PURPOSE
        "KDDockWidgets is a Qt dock widget library written by KDAB, suitable for replacing QDockWidget and implementing
 advanced functionalities missing in Qt. https://github.com/KDAB/KDDockWidgets"
    TYPE REQUIRED
)

find_package(QCustomPlot)
set_package_properties(
    QCustomPlot PROPERTIES
    DESCRIPTION "QCustomPlot is a Qt charting library. https://www.qcustomplot.com/"
    PURPOSE "Frequency tab"
    URL "https://www.qcustomplot.com/index.php/download"
    TYPE OPTIONAL
)

find_package(KGraphViewerPart CONFIG)
set_package_properties(
    KGraphViewerPart PROPERTIES
    DESCRIPTION "KGraphViewer (from KDE extragear) is a tool to display graphviz .dot graphs"
    PURPOSE "Call graph in the caller/callee tab"
    URL "https://invent.kde.org/graphics/kgraphviewer"
    TYPE OPTIONAL
)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMCoverageOption)
include(ECMEnableSanitizers)

file(RELATIVE_PATH LIBEXEC_REL_PATH "${KDE_INSTALL_FULL_BINDIR}" "${KDE_INSTALL_FULL_LIBEXECDIR}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/hotspot-config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/hotspot-config.h @ONLY)

add_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_USE_QSTRINGBUILDER)
add_compile_options(-Wall -pedantic -Wextra)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
endif()

add_subdirectory(3rdparty)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(src)

add_subdirectory(tests)

install(
    FILES com.kdab.hotspot.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)
install(
    FILES com.kdab.Hotspot.appdata.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
