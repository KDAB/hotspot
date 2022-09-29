add_executable(
    cpp-minimal-static
    main.cpp
)
target_link_libraries(
    cpp-minimal-static "-static"
)
