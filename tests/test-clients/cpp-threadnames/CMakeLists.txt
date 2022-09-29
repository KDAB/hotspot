add_executable(
    cpp-threadnames
    main.cpp
)
target_link_libraries(
    cpp-threadnames Threads::Threads
)
