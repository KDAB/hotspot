if (OBJDUMP_PROGRAM)
  set (objdump_FIND_QUIETLY TRUE)
endif()

find_program(OBJDUMP_PROGRAM
    NAMES
        objdump
    HINTS
        ${CMAKE_OBJDUMP})

include (FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set objdump_FOUND to TRUE if all listed variables are TRUE
find_package_handle_standard_args(objdump DEFAULT_MSG
    OBJDUMP_PROGRAM)

mark_as_advanced(OBJDUMP_PROGRAM)
