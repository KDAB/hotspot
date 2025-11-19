vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/threadweaver
    REF "v${VERSION}"
    SHA512 5e206aae78d7aac291c55c106b91c49dbb184635af4295e256ef3c3cb025a3107f83cc3de581dea35abcd29acc1648746acd6daa2638f48d8539de72b14a273e
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6ThreadWeaver CONFIG_PATH lib/cmake/KF6ThreadWeaver)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
