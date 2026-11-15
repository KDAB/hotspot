vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDAB/KDDockWidgets
    REF "v${VERSION}"
    SHA512 1e220c5cf608c5bb9242b530eb1e45a15dae462b126c12d253483a1213e72374baa75943d8734c5dc79e34b03b480d1a87cd59cb945996abc0ab20b5d649a5cb
    HEAD_REF main
)

# Determine frontends from features
set(FRONTENDS "")
if("qtwidgets" IN_LIST FEATURES)
    list(APPEND FRONTENDS "qtwidgets")
endif()
list(JOIN FRONTENDS ";" FRONTENDS_STRING)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(KDDW_STATIC ON)
else()
    set(KDDW_STATIC OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DKDDockWidgets_QT6=ON
        -DKDDockWidgets_STATIC=${KDDW_STATIC}
        # Don't pick up system copies, they'd leak into the exported link interface
        -DKDDockWidgets_NO_SPDLOG=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_spdlog=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=ON
        -DKDDockWidgets_FRONTENDS=${FRONTENDS_STRING}
        -DKDDockWidgets_EXAMPLES=OFF
        -DKDDockWidgets_TESTS=OFF
        -DKDDockWidgets_DOCS=OFF
        -DKDDockWidgets_PYTHON_BINDINGS=OFF
        -DKDDockWidgets_DEVELOPER_MODE=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME KDDockWidgets-qt6
    CONFIG_PATH lib/cmake/KDDockWidgets-qt6
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
