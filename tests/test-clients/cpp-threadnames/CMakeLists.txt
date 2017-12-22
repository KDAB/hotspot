set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-threadnames main.cpp)
target_link_libraries(cpp-threadnames Threads::Threads)
