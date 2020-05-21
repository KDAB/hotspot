add_library(PrefixTickLabels STATIC
    PrefixTickLabels/src/PrefixTickLabels.cpp
    )
target_include_directories(PrefixTickLabels PUBLIC
    PrefixTickLabels/src
    )
target_link_libraries(PrefixTickLabels PUBLIC
    Qt5::Core
    )
