vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kguiaddons
    REF "v${VERSION}"
    SHA512 31572d02edd8ac6d01f7ddbb0bbbc0b9eb4a708f41ec2c62d6d4b0dee1400a0cb5154621a9146bc1b87093586e2440ad2405ef3c6b2e6c7dbf32b518b8ac72be
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
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6GuiAddons CONFIG_PATH lib/cmake/KF6GuiAddons)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
