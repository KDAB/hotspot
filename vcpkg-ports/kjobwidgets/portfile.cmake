vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kjobwidgets
    REF "v${VERSION}"
    SHA512 e22ff45539094dbeae3068d506c148016fb7d10795f9bd54001128ca092ae47b935e2d84b2f860c78a4051d7011f20b25628c12f7483dd14810a5e836d19de18
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
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6JobWidgets CONFIG_PATH lib/cmake/KF6JobWidgets)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")

# Fix missing find_dependency calls for static library LINK_ONLY dependencies
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/KF6JobWidgets/KF6JobWidgetsConfig.cmake"
    "find_dependency(KF6CoreAddons"
    "find_dependency(KF6WidgetsAddons \"6.20.0\")\nfind_dependency(KF6Notifications \"6.20.0\")\nfind_dependency(KF6CoreAddons"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt")
