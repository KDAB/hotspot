set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(cpp-std-symbols main.cpp)
add_executable(cpp-std-symbols-98 main.cpp)

set_property(TARGET cpp-std-symbols-98 PROPERTY CXX_STANDARD 98)
target_compile_definitions(cpp-std-symbols-98 PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
