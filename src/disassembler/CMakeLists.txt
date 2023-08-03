add_library(
    disassembler STATIC
    disassemble.cpp dwarfdump.cpp
)

target_link_libraries(
    disassembler
    PUBLIC ${LIBDW_LIBRARIES} Qt::Core models
)
target_include_directories(
    disassembler
    PUBLIC ${LIBDWARF_INCLUDE_DIRS}
)
