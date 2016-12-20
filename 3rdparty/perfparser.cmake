include_directories(
    ${LIBELF_INCLUDE_DIR}
    ${LIBDW_INCLUDE_DIR}/elfutils
)

add_executable(hotspot-perfparser
    perfparser/app/main.cpp
    perfparser/app/perfattributes.cpp
    perfparser/app/perfheader.cpp
    perfparser/app/perffilesection.cpp
    perfparser/app/perffeatures.cpp
    perfparser/app/perfdata.cpp
    perfparser/app/perfunwind.cpp
    perfparser/app/perfregisterinfo.cpp
    perfparser/app/perfstdin.cpp
    perfparser/app/perfsymboltable.cpp
)

target_link_libraries(hotspot-perfparser
    Qt5::Core
    Qt5::Network
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
)

set_target_properties(hotspot-perfparser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${LIBEXEC_INSTALL_DIR}"
)

install(TARGETS hotspot-perfparser RUNTIME DESTINATION ${LIBEXEC_INSTALL_DIR})
