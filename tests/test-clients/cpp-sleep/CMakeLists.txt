set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-sleep main.cpp)
target_link_libraries(cpp-sleep Threads::Threads)
