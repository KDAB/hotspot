add_executable(
    cpp-libs
    main.cpp
)
add_library(
    cpp-libs-static STATIC
    staticlib.cpp
)
add_library(
    cpp-libs-shared SHARED
    sharedlib.cpp
)
target_link_libraries(
    cpp-libs cpp-libs-static cpp-libs-shared
)
