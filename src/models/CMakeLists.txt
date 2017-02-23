add_library(models STATIC
    treemodel.cpp
    topproxy.cpp
    data.cpp
    callercalleemodel.cpp
)

target_link_libraries(models
    Qt5::Core
)
