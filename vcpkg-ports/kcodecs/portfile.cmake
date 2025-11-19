vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kcodecs
    REF "v${VERSION}"
    SHA512 6d6dd7dd30b0206f81c5a54a289a4efda23e04efdd707f320939a03e625100729f0673a35081958d3feafc3678c036ed6807c9303a063f7cb5dbd0da01338ea4
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6Codecs CONFIG_PATH lib/cmake/KF6Codecs)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
