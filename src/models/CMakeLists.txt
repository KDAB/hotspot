add_library(models STATIC
    treemodel.cpp
    topproxy.cpp
    data.cpp
    callercalleemodel.cpp
    costdelegate.cpp
    timelinedelegate.cpp
    eventmodel.cpp
    ../util.cpp
)

target_link_libraries(models
    Qt5::Core
    Qt5::Widgets
    KF5::ConfigWidgets
)
