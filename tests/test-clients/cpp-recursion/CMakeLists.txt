add_executable(
    cpp-recursion
    main.cpp
)

set_target_properties(
    cpp-recursion PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${KDE_INSTALL_EXECROOTDIR}/tests/test-clients/cpp-recursion"
)
