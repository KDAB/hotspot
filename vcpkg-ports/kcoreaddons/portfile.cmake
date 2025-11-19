vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kcoreaddons
    REF "v${VERSION}"
    SHA512 1047d0de1800bcde14fc768bf598ecd26d5d339c8380e97c9dca759e9f81b837a87fd36515aef95f650735df2afa1f5b8f74c3ea3119ad489bf1821c73d5c3b7
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
        -DBUILD_PYTHON_BINDINGS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6CoreAddons CONFIG_PATH lib/cmake/KF6CoreAddons)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
