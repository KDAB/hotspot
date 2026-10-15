# SPDX-FileCopyrightText: 2025 Sergio Martins
#
# SPDX-License-Identifier: MIT

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_FIXUP_ELF_RPATH ON)

set(VCPKG_C_FLAGS "-fsanitize=thread")
set(VCPKG_CXX_FLAGS "-fsanitize=thread")
set(VCPKG_LINKER_FLAGS "-fsanitize=thread")
