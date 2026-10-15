vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/knotifications
    REF "v${VERSION}"
    SHA512 176130f3e4edf4215aa0c61b246a313cbcbe1b05bb9c27300a4cfcc91c9a91d5c066ffc009d9f2cf65e4b39c80fa0ab042609750e5f533f550d630b049d2dd56
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
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6Notifications CONFIG_PATH lib/cmake/KF6Notifications)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
