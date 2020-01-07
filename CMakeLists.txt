cmake_minimum_required(VERSION 3.1.0)
project(hotspot VERSION 1.2.80 LANGUAGES CXX C)

if (POLICY CMP0063)
    cmake_policy (SET CMP0063 NEW)
endif (POLICY CMP0063)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/perfparser/app/main.cpp" )
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        message( FATAL_ERROR
            "The perfparser git submodule is not initialized.\n"
            "Please run the following commands in the source directory (${PROJECT_SOURCE_DIR}):\n"
            "    git submodule update --init --recursive\n"
        )
    else()
        message( FATAL_ERROR
            "The perfparser submodule is missing - please report a broken source package.\n"
        )
    endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ ${CMAKE_MODULE_PATH})
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

set(CMAKE_CXX_STANDARD 11)

# Enable the test harness
enable_testing()

# Version setup
set(HOTSPOT_VERSION_STRING "${hotspot_VERSION}")

include(FeatureSummary)

find_package(Qt5 5.7 COMPONENTS Core Widgets Network Test Svg REQUIRED)
find_package(LibElf REQUIRED)
find_package(Elfutils REQUIRED) # TODO: make optional, use internal copy
find_package(ECM 1.0.0 NO_MODULE REQUIRED)
find_package(Threads REQUIRED)

if(Qt5_FOUND AND ECM_FOUND)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
    find_package(KF5 COMPONENTS
        ThreadWeaver
        I18n
        ConfigWidgets
        CoreAddons
        ItemViews
        ItemModels
        KIO
        Solid
        WindowSystem
        REQUIRED
    )
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMCoverageOption)
include(ECMEnableSanitizers)

file(RELATIVE_PATH LIBEXEC_REL_PATH
   "${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_BINDIR}"
   "${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_LIBEXECDIR}")

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
