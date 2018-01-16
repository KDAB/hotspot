set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-parallel main.cpp)
target_link_libraries(cpp-parallel Threads::Threads)
