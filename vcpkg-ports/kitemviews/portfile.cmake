vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kitemviews
    REF "v${VERSION}"
    SHA512 47ab166e6c6ba1ecc66c4e5acca97fcfaf715c2fd9790d4dfa988392c1271ebb8fd508e9829b5fa41733085281d2e2e6b622b3a6df1f97c4ac4d954c2a7c24e2
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6ItemViews CONFIG_PATH lib/cmake/KF6ItemViews)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
