vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kiconthemes
    REF "v${VERSION}"
    SHA512 5534dcde7cec52e590ea1fed9451e301203c0401bec0719f21cf899a8dbded1d60dca623b46c94773d51f7e417c622065c178eb5c8be73f3febafb8f26421145
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DKDE_CLANG_FORMAT_EXECUTABLE=
        -DUSE_BreezeIcons=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6IconThemes CONFIG_PATH lib/cmake/KF6IconThemes)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
