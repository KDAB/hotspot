add_library(models STATIC
    costmodel.cpp
    topproxy.cpp
    framedata.cpp
    callercalleemodel.cpp
)

target_link_libraries(models
    Qt5::Core
)
