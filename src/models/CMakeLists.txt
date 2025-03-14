add_library(
    models STATIC
    ../settings.cpp
    ../util.cpp
    byfilemodel.cpp
    callercalleemodel.cpp
    callercalleeproxy.cpp
    codedelegate.cpp
    costdelegate.cpp
    data.cpp
    disassemblymodel.cpp
    disassemblyoutput.cpp
    eventmodel.cpp
    eventmodelproxy.cpp
    filterandzoomstack.cpp
    formattingutils.cpp
    frequencymodel.cpp
    highlightedtext.cpp
    processfiltermodel.cpp
    processlist_unix.cpp
    processmodel.cpp
    sourcecodemodel.cpp
    timeaxisheaderview.cpp
    timelinedelegate.cpp
    topproxy.cpp
    tracepointformat.cpp
    treemodel.cpp
)

target_link_libraries(
    models
    fmt_parser
    Qt::Core
    Qt::Widgets
    KF${QT_MAJOR_VERSION}::ItemModels
    KF${QT_MAJOR_VERSION}::ConfigWidgets
    KF${QT_MAJOR_VERSION}::Parts
    PrefixTickLabels
)

if(KFSyntaxHighlighting_FOUND)
    target_link_libraries(models KF${QT_MAJOR_VERSION}::SyntaxHighlighting)
endif()
