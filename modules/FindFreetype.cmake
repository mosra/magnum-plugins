#.rst:
# FindFreetype
# ------------
#
# Locate FreeType library
#
# This module defines
#
# ::
#
#   FREETYPE_LIBRARIES, the library to link against
#   FREETYPE_DEPENDENCY_LIBRARIES, freetype's dependencies, if needed
#   FREETYPE_FOUND, if false, do not try to link to FREETYPE
#   FREETYPE_INCLUDE_DIRS, where to find headers.
#   FREETYPE_VERSION_STRING, the version of freetype found (since CMake 2.8.8)
#   This is the concatenation of the paths:
#   FREETYPE_INCLUDE_DIR_ft2build
#   FREETYPE_INCLUDE_DIR_freetype2
#
#
#
# $FREETYPE_DIR is an environment variable that would correspond to the
# ./configure --prefix=$FREETYPE_DIR used in building FREETYPE.

#=============================================================================
# CMake - Cross Platform Makefile Generator
# Copyright 2000-2016 Kitware, Inc.
# Copyright 2000-2011 Insight Software Consortium
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
#   nor the names of their contributors may be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================


# Created by Eric Wing.
# Modifications by Alexander Neundorf.
# This file has been renamed to "FindFreetype.cmake" instead of the correct
# "FindFreeType.cmake" in order to be compatible with the one from KDE4, Alex.

# This version is modified for Magnum, and was forked from
# https://github.com/Kitware/CMake/blob/b213a7f6ab0d4aa18e7b704bf1cf4994fae77254/Modules/FindFreetype.cmake
# The file was modified to detect if Freetype was built as static or shared,
# and if so, detect which dependencies it was built with and require them with
# find_package, finally exposing them for linking through a new variable.

# Ugh, FreeType seems to use some #include trickery which
# makes this harder than it should be. It looks like they
# put ft2build.h in a common/easier-to-find location which
# then contains a #include to a more specific header in a
# more specific location (#include <freetype/config/ftheader.h>).
# Then from there, they need to set a bunch of #define's
# so you can do something like:
# #include FT_FREETYPE_H
# Unfortunately, using CMake's mechanisms like include_directories()
# wants explicit full paths and this trickery doesn't work too well.
# I'm going to attempt to cut out the middleman and hope
# everything still works.

set(FREETYPE_FIND_ARGS
  HINTS
    ENV FREETYPE_DIR
  PATHS
    /usr/X11R6
    /usr/local/X11R6
    /usr/local/X11
    /usr/freeware
    ENV GTKMM_BASEPATH
    [HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\2.4;Path]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\2.4;Path]
)

find_path(
  FREETYPE_INCLUDE_DIR_ft2build
  ft2build.h
  ${FREETYPE_FIND_ARGS}
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

find_path(
  FREETYPE_INCLUDE_DIR_freetype2
  NAMES
    freetype/config/ftheader.h
    config/ftheader.h
  ${FREETYPE_FIND_ARGS}
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

if(NOT FREETYPE_LIBRARY)
  find_library(FREETYPE_LIBRARY_RELEASE
    NAMES
      freetype
      libfreetype
      freetype219
    ${FREETYPE_FIND_ARGS}
    PATH_SUFFIXES
      lib
  )
  find_library(FREETYPE_LIBRARY_DEBUG
    NAMES
      freetyped
      libfreetyped
      freetype219d
    ${FREETYPE_FIND_ARGS}
    PATH_SUFFIXES
      lib
  )
  include(SelectLibraryConfigurations)
  select_library_configurations(FREETYPE)
endif()

unset(FREETYPE_FIND_ARGS)

# set the user variables
if(FREETYPE_INCLUDE_DIR_ft2build AND FREETYPE_INCLUDE_DIR_freetype2)
  set(FREETYPE_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIR_ft2build};${FREETYPE_INCLUDE_DIR_freetype2}")
  list(REMOVE_DUPLICATES FREETYPE_INCLUDE_DIRS)
endif()
set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}")

if(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
elseif(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
endif()

if(FREETYPE_INCLUDE_DIR_freetype2 AND FREETYPE_H)
  file(STRINGS "${FREETYPE_H}" freetype_version_str
       REGEX "^#[\t ]*define[\t ]+FREETYPE_(MAJOR|MINOR|PATCH)[\t ]+[0-9]+$")

  unset(FREETYPE_VERSION_STRING)
  foreach(VPART MAJOR MINOR PATCH)
    foreach(VLINE ${freetype_version_str})
      if(VLINE MATCHES "^#[\t ]*define[\t ]+FREETYPE_${VPART}[\t ]+([0-9]+)$")
        set(FREETYPE_VERSION_PART "${CMAKE_MATCH_1}")
        if(FREETYPE_VERSION_STRING)
          set(FREETYPE_VERSION_STRING "${FREETYPE_VERSION_STRING}.${FREETYPE_VERSION_PART}")
        else()
          set(FREETYPE_VERSION_STRING "${FREETYPE_VERSION_PART}")
        endif()
        unset(FREETYPE_VERSION_PART)
      endif()
    endforeach()
  endforeach()
endif()

# handle the QUIETLY and REQUIRED arguments and set FREETYPE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  Freetype
  REQUIRED_VARS
    FREETYPE_LIBRARY
    FREETYPE_INCLUDE_DIRS
  VERSION_VAR
    FREETYPE_VERSION_STRING
)

if(FREETYPE_FOUND)
  # Determine if Freetype was built as static or shared
  set(FREETYPE_SHARED OFF)
  foreach(_path ${FREETYPE_LIBRARIES})
    string(FIND ${_path} "/" _result REVERSE)
    string(SUBSTRING ${_path} ${_result} -1 _name)
    foreach(_hint dll so dylib)
      string(FIND ${_name} ${_hint} _result)
      if(NOT _result EQUAL -1)
        set(FREETYPE_SHARED ON)
        break()
      endif()
    endforeach()
  endforeach()

  # If Freetype was built static, we need to link its optional dependencies
  if(NOT FREETYPE_SHARED)
    # We can read which dependencies Freetype was built with from ftoption.h
    file(READ "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/config/ftoption.h" _ftoption_h)
    foreach(_dep ZLIB BZip2 PNG HarfBuzz)
      string(TOUPPER ${_dep} _DEP)
      set(_require_dep OFF)

      # Use regex to check if a depdency is needed
      # Example of when a dependency was used to build Freetype:
      # #define FT_CONFIG_OPTION_USE_BZIP2
      # Example of when a dependency was not used to build Freetype:
      # /* #define FT_CONFIG_OPTION_USE_HARFBUZZ */
      # We want to match the first and not the second
      set(_match_before "\n[ \t]*\\#define[ \t]*FT_CONFIG_OPTION")
      set(_match_after "[ \t]*\n")
      string(REGEX MATCH "${_match_before}_USE_${_DEP}${_match_after}" _use_dep "${_ftoption_h}")
      if("${_dep}" STREQUAL "ZLIB")
        # Special handling
        string(REGEX MATCH "${_match_before}_SYSTEM_${_DEP}${_match_after}" _sys_dep "${_ftoption_h}")
        if(NOT "${_use_dep}" STREQUAL "" AND NOT "${_sys_dep}" STREQUAL "")
          set(_require_dep ON)
        endif()
      else()
        if(NOT "${_use_dep}" STREQUAL "")
          set(_require_dep ON)
        endif()
      endif()

      # Require the dependency only if Freetype was built with it
      if(_require_dep)
        find_package(${_dep} QUIET REQUIRED)
        list(APPEND FREETYPE_DEPENDENCY_LIBRARIES ${${_DEP}_LIBRARIES})
      endif()
    endforeach()
  endif()
endif()

mark_as_advanced(
  FREETYPE_INCLUDE_DIR_freetype2
  FREETYPE_INCLUDE_DIR_ft2build
)
