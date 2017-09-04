set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-locking main.cpp)
target_link_libraries(cpp-locking Threads::Threads)
