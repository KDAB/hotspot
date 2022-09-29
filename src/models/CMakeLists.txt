add_library(
    models STATIC
    ../settings.cpp
    ../util.cpp
    callercalleemodel.cpp
    callercalleeproxy.cpp
    codedelegate.cpp
    costdelegate.cpp
    data.cpp
    disassemblymodel.cpp
    disassemblyoutput.cpp
    eventmodel.cpp
    filterandzoomstack.cpp
    frequencymodel.cpp
    highlighter.cpp
    processfiltermodel.cpp
    processlist_unix.cpp
    processmodel.cpp
    sourcecodemodel.cpp
    timeaxisheaderview.cpp
    timelinedelegate.cpp
    topproxy.cpp
    treemodel.cpp
)

target_link_libraries(
    models
    Qt5::Core
    Qt5::Widgets
    KF5::ItemModels
    KF5::ConfigWidgets
    KF5::Parts
    PrefixTickLabels
)

if(KF5SyntaxHighlighting_FOUND)
    target_link_libraries(models KF5::SyntaxHighlighting)
endif(KF5SyntaxHighlighting_FOUND)
