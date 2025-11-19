vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kitemmodels
    REF "v${VERSION}"
    SHA512 a3f9bf3f4fee31d4a57b43b21b9c7c4f38416c0ae1b852464c6ae30282176d3f8cd2353bd644090c69a560567e98ebda2871f8e2a07e381b03dcc6eb96875f1f
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6ItemModels CONFIG_PATH lib/cmake/KF6ItemModels)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
