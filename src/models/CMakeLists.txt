add_library(models STATIC
    costmodel.cpp
    topproxy.cpp
    framedata.cpp
)

target_link_libraries(models
    Qt5::Core
)
