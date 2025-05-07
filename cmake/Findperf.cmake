if (PERF_PROGRAM)
  set (PERF_FIND_QUIETLY TRUE)
endif()

find_program(PERF_PROGRAM NAMES perf)

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set perf_FOUND to TRUE if all listed variables are TRUE
find_package_handle_standard_args(perf DEFAULT_MSG
    PERF_PROGRAM)

mark_as_advanced(PERF_PROGRAM)
