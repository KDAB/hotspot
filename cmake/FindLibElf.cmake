# - Try to find libelf
# Once done this will define
#
#  LIBELF_FOUND - system has libelf
#  LIBELF_INCLUDE_DIRS - the libelf include directory
#  LIBELF_LIBRARIES - Link these to use libelf
#  LIBELF_DEFINITIONS - Compiler switches required for using libelf
#
#  Copyright (c) 2008 Bernhard Walle <bernhard.walle@gmx.de>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (LIBELF_LIBRARIES AND LIBELF_INCLUDE_DIRS)
  set (LibElf_FIND_QUIETLY TRUE)
endif (LIBELF_LIBRARIES AND LIBELF_INCLUDE_DIRS)

find_path (LIBELF_INCLUDE_DIRS
    NAMES
      libelf.h
    PATHS
      /usr/include
      /usr/include/libelf
      /usr/local/include
      /usr/local/include/libelf
      /opt/local/include
      /opt/local/include/libelf
      /sw/include
      /sw/include/libelf
      ENV CPATH)

find_library (LIBELF_LIBRARIES
    NAMES
      elf
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ENV LIBRARY_PATH
      ENV LD_LIBRARY_PATH)

include (FindPackageHandleStandardArgs)


# handle the QUIETLY and REQUIRED arguments and set LIBELF_FOUND to TRUE if all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibElf DEFAULT_MSG
    LIBELF_LIBRARIES
    LIBELF_INCLUDE_DIRS)


mark_as_advanced(LIBELF_INCLUDE_DIRS LIBELF_LIBRARIES)
