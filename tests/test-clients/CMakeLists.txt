include(CMakePushCheckState)

# Check if static linking works at all.
# For example OpenSuse 42.3 with clang doesn't ship a static libc++.a,
# making static linking fail.
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES "-static")
check_cxx_source_compiles("int main() {}" STATIC_LINKING_WORKS)
cmake_pop_check_state()

if (STATIC_LINKING_WORKS)
    add_subdirectory(cpp-minimal-static)
endif()
add_subdirectory(cpp-inlining)
add_subdirectory(cpp-recursion)
add_subdirectory(cpp-locking)
add_subdirectory(cpp-stdin)
add_subdirectory(c-syscalls)
add_subdirectory(cpp-sleep)
add_subdirectory(cpp-threadnames)
add_subdirectory(cpp-parallel)
