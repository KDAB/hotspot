vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kwindowsystem
    REF "v${VERSION}"
    SHA512 1681202d9265a23cf8f3a0c8b60142c314da18b12e013e8f210d298c14cdc21617ccd2e2465c2482544973ea91e24d90a76f185d074bb44b95ba5733197185b5
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6WindowSystem CONFIG_PATH lib/cmake/KF6WindowSystem)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.1-or-later.txt")
