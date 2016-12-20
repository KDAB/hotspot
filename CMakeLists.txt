project(hotspot CXX C)

cmake_minimum_required(VERSION 3.1.0)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ ${CMAKE_MODULE_PATH})
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)

# Enable the test harness
enable_testing()

# Version setup
set(HOTSPOT_VERSION_MAJOR "0")
set(HOTSPOT_VERSION_MINOR "0")
set(HOTSPOT_VERSION_PATCH "80")
set(HOTSPOT_VERSION "${HOTSPOT_VERSION_MAJOR}.${HOTSPOT_VERSION_MINOR}.${HOTSPOT_VERSION_PATCH}")
set(HOTSPOT_VERSION_STRING "${HOTSPOT_VERSION}")
set(HOTSPOT_SOVERSION "1.0.0")
set(HOTSPOT_PLUGIN_VERSION "1.0")
set(PROJECT_VERSION_STRING "${HOTSPOT_VERSION_STRING}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/hotspot-config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/hotspot-config.h
  @ONLY
)

include(FeatureSummary)

find_package(Qt5 COMPONENTS Core Widgets Network REQUIRED)
find_package(LibElf REQUIRED)
find_package(Elfutils REQUIRED) # TODO: make optional, use internal copy
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

add_subdirectory(3rdparty)
add_subdirectory(src)
