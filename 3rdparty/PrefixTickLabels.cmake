add_library(PrefixTickLabels STATIC
    PrefixTickLabels/src/PrefixTickLabels.cpp
    )
include_directories(
    PrefixTickLabels/src
    )
target_include_directories(PrefixTickLabels PUBLIC
    PrefixTickLabels/src
    )
target_link_libraries(PrefixTickLabels
    Qt5::Core
    )


