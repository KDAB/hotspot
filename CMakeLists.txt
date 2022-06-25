cmake_minimum_required(VERSION 3.16)
project(hotspot VERSION 1.3.80 LANGUAGES CXX C)

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/perfparser/cmake/
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/
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
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable the test harness
enable_testing()

# Version setup
set(HOTSPOT_VERSION_STRING "${hotspot_VERSION}")

include(HotspotMacrosInternal)
include(FeatureSummary)

find_package(Qt5 5.15 COMPONENTS Core Widgets Network Test Svg REQUIRED)
find_package(LibElf REQUIRED)
find_package(ElfUtils REQUIRED)
find_package(ECM 1.0.0 NO_MODULE REQUIRED)
find_package(Threads REQUIRED)
find_package(Zstd)
find_package(objdump)
set_package_properties(objdump PROPERTIES
        DESCRIPTION "Utility to generate disassembler"
        PURPOSE "Support for annotating individual instructions"
        URL "https://www.gnu.org/software/binutils/"
        TYPE RUNTIME)

find_package(LibRustcDemangle)
set_package_properties(LibRustcDemangle PROPERTIES
        DESCRIPTION "Demangling for Rust symbols, written in Rust."
        PURPOSE "Demangling of Rust symbols"
        URL "https://github.com/alexcrichton/rustc-demangle"
        TYPE RUNTIME)

find_package(LibDDemangle)
set_package_properties(LibDDemangle PROPERTIES
	DESCRIPTION "Demangling for D symbols, written in D."
	PURPOSE "Demangling of D symbols"
        URL "https://github.com/lievenhey/d_demangler"
        TYPE RUNTIME)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
find_package(KF5 COMPONENTS
    ThreadWeaver
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
    REQUIRED
)

find_package(KF5Archive 5.85.0 CONFIG)
find_package(KF5Auth CONFIG)
find_package(KDDockWidgets CONFIG)
set_package_properties(KDDockWidgets PROPERTIES
    PURPOSE "KDDockWidgets is a Qt dock widget library written by KDAB, suitable for replacing QDockWidget and implementing
 advanced functionalities missing in Qt. https://github.com/KDAB/KDDockWidgets"
    TYPE REQUIRED
)

find_package(QCustomPlot)
set_package_properties(QCustomPlot PROPERTIES
    PURPOSE "QCustomPlot is a Qt charting library. https://www.qcustomplot.com/"
    TYPE OPTIONAL
)

find_program(KGRAPHVIEWER_FOUND kgraphviewer)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMCoverageOption)
include(ECMEnableSanitizers)

file(RELATIVE_PATH LIBEXEC_REL_PATH
   "${KDE_INSTALL_FULL_BINDIR}"
   "${KDE_INSTALL_FULL_LIBEXECDIR}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/hotspot-config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/hotspot-config.h
  @ONLY
)

add_subdirectory(3rdparty)
add_subdirectory(scripts)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(src)

add_subdirectory(tests)

install(FILES com.kdab.hotspot.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES com.kdab.Hotspot.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
