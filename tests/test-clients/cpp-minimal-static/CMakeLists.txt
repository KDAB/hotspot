set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS) #this removes -rdynamic which can sometimes cause problems

add_executable(cpp-minimal-static main.cpp)
target_link_libraries(cpp-minimal-static "-static")
