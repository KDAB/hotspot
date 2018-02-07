set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-minimal-static main.cpp)
target_link_libraries(cpp-minimal-static "-static")
