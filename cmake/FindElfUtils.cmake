# - Try to find libdwarf
# Once done this will define
#
#  LIBDWARF_FOUND - system has libdwarf
#  LIBDWARF_INCLUDE_DIRS - the libdwarf include directory
#  LIBDWARF_LIBRARIES - Link these to use libdwarf
#  LIBDWARF_DEFINITIONS - Compiler switches required for using libdwarf
#

if (LIBDWARF_LIBRARIES AND LIBDWARF_INCLUDE_DIRS)
  set (LibDwarf_FIND_QUIETLY TRUE)
endif (LIBDWARF_LIBRARIES AND LIBDWARF_INCLUDE_DIRS)

find_path (DWARF_INCLUDE_DIR
    NAMES
      dwarf.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      ENV CPATH) # PATH and INCLUDE will also work
find_path (LIBDW_INCLUDE_DIR
    NAMES
      elfutils/libdw.h elfutils/libdwfl.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      ENV CPATH)
if (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR)
    set (LIBDWARF_INCLUDE_DIRS  ${DWARF_INCLUDE_DIR} ${LIBDW_INCLUDE_DIR})
endif (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR)

find_library (LIBDW_LIBRARIES
    NAMES
      dw
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ENV LIBRARY_PATH   # PATH and LIB will also work
      ENV LD_LIBRARY_PATH)

include (FindPackageHandleStandardArgs)


# handle the QUIETLY and REQUIRED arguments and set LIBDWARF_FOUND to TRUE
# if all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ElfUtils DEFAULT_MSG
    LIBDW_LIBRARIES
    LIBDW_INCLUDE_DIR)

mark_as_advanced(LIBDW_INCLUDE_DIR LIBDW_LIBRARIES)
