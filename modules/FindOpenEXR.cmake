#.rst:
# Find OpenEXR
# ------------
#
# Finds the OpenEXR library, either by delegating to the OpenEXR CMake config
# (if found), or by falling back to plain filesystem search. This module
# supplements both IlmBase and OpenEXR config files and defines:
#
#  OpenEXR_FOUND        - True if the OpenEXR library is found
#  OpenEXR::IlmImf      - libIlmImf imported target
#
#  IlmBase::Half        - libHalf imported target
#  IlmBase::Iex         - libIex imported target
#  IlmBase::IlmThread   - libIlmThread imported target
#  IlmBase::Imath       - libImath imported target
#
# The IexMath library is not being searched for as it's not needed to be linked
# to and it no longer exists in OpenEXR 3.0.0. Additionally these variables are
# defined for internal usage:
#
#  OPENEXR_ILMINF_LIBRARY - libIlmInf library
#  OPENEXR_INCLUDE_DIR    - Include dir
#
#  ILMBASE_HALF_LIBRARY - libHalf library
#  ILMBASE_IEX_LIBRARY  - libIex library
#  ILMBASE_ILMTHREAD_LIBRARY - libIlmThread library
#  ILMBASE_IMATH_LIBRARY - libImath library
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

# OpenEXR as a CMake subproject. Nothing left to do here (we can't do anything
# with it (such as fixing the include path) until CMake 3.13 as the alias is
# defined in another source dir).
if(TARGET OpenEXR::OpenEXR)
    # Just to make FPHSA print some meaningful location, nothing else
    get_target_property(_OPENEXR_DIRECTORY OpenEXR::OpenEXR SOURCE_DIR)
    get_filename_component(_OPENEXR_DIRECTORY ${_OPENEXR_DIRECTORY} DIRECTORY)
    get_filename_component(_OPENEXR_DIRECTORY ${_OPENEXR_DIRECTORY} DIRECTORY)
    get_filename_component(_OPENEXR_DIRECTORY ${_OPENEXR_DIRECTORY} DIRECTORY)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(OpenEXR DEFAULT_MSG _OPENEXR_DIRECTORY)

    return()
endif()

# If OpenEXR is not a CMake subproject already, try to find it via its config
# files. These are available only since 2.5 (2.4?), for older versions we'll
# have to fall back to finding ourselves. Also do that only on CMake 3.9 and
# greater, because the config files call find_dependency() from
# CMakeFindDependencyMacro with 3 arguments, which isn't supported before:
# https://github.com/Kitware/CMake/commit/ab358d6a859d8b7e257ed1e06ca000e097a32ef6
if(NOT CMAKE_VERSION VERSION_LESS 3.9.0)
    find_package(OpenEXR CONFIG QUIET)
    if(OpenEXR_FOUND)
        # OpenEXR before version 3.0 has some REALLY WEIRD target names. Alias
        # them to the saner new targets.
        if(NOT TARGET OpenEXR::OpenEXR)
            add_library(OpenEXR::OpenEXR INTERFACE IMPORTED)
            set_target_properties(OpenEXR::OpenEXR PROPERTIES
                INTERFACE_LINK_LIBRARIES OpenEXR::IlmImf)
            get_target_property(_OPENEXR_INTERFACE_INCLUDE_DIRECTORIES OpenEXR::IlmImf INTERFACE_INCLUDE_DIRECTORIES)
        else()
            get_target_property(_OPENEXR_INTERFACE_INCLUDE_DIRECTORIES OpenEXR::OpenEXR INTERFACE_INCLUDE_DIRECTORIES)
        endif()

        # Just to make FPHSA print some meaningful location, nothing else
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(OpenEXR DEFAULT_MSG
            _OPENEXR_INTERFACE_INCLUDE_DIRECTORIES)

        return()
    endif()
endif()

# ZLIB and threads are a dependency. Can't use CMakeFindDependencyMacro because
# "The call to return() makes this macro unsuitable to call from Find Modules".
# Thanks for nothing, CMake.
if(OpenEXR_FIND_QUIETLY)
    set(_OPENEXR_FIND_QUIET QUIET)
else()
    set(_OPENEXR_FIND_QUIET )
endif()
if(OpenEXR_FIND_REQUIRED)
    set(_OPENEXR_FIND_REQUIRED REQUIRED)
else()
    set(_OPENEXR_FIND_REQUIRED )
endif()
find_package(ZLIB ${_OPENEXR_FIND_QUIET} ${_OPENEXR_FIND_REQUIRED})
find_package(Threads ${_OPENEXR_FIND_QUIET} ${_OPENEXR_FIND_REQUIRED})

# All the libraries. Don't bother with debug/release distinction, if people
# have that, there's a high chance they have a non-ancient version with CMake
# configs as well... except if they are on CMake <3.9, in which case they
# can't. Not my problem.
#
# Also, imagine that, of all things that are possible with SOVERSION, you
# choose to embed the version string IN THE FILENAME!!! When finding we prefer
# a library without a suffix (which is the modicum of sanity distro packagers
# add back to this lib) and then try from the latest to oldest in case someone
# would really be so crazy and have multiple versions installed together.
find_library(OPENEXR_ILMIMF_LIBRARY NAMES
    IlmImf
    IlmImf-3_0
    IlmImf-2_5
    IlmImf-2_4
    IlmImf-2_3
    IlmImf-2_2)
find_library(ILMBASE_HALF_LIBRARY NAMES
    Half
    Half-3_0
    Half-2_5
    Half-2_4
    Half-2_3
    Half-2_2)
find_library(ILMBASE_IEX_LIBRARY NAMES
    Iex
    Iex-3_0
    Iex-2_5
    Iex-2_4
    Iex-2_3
    Iex-2_2)
find_library(ILMBASE_ILMTHREAD_LIBRARY NAMES
    IlmThread
    IlmThread-3_0
    IlmThread-2_5
    IlmThread-2_4
    IlmThread-2_3
    IlmThread-2_2)
find_library(ILMBASE_IMATH_LIBRARY NAMES
    Imath
    Imath-3_0
    Imath-2_5
    Imath-2_4
    Imath-2_3
    Imath-2_2)

# Include dir. Unfortunately we can't use #include <OpenEXR/blah> because in
# case of OpenEXR being a CMake subproject, OpenEXR::OpenEXR defines the
# include dirs *inside* the subdirectories and there's no way to change that
# from outside until CMake 3.13. This means one extra hurdle for 3rd party
# buildsystems where they have to add OpenEXR/ to include path. Sorry.
find_path(OPENEXR_INCLUDE_DIR NAMES ImfHeader.h PATH_SUFFIXES OpenEXR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenEXR DEFAULT_MSG
    OPENEXR_ILMIMF_LIBRARY
    OPENEXR_INCLUDE_DIR
    ILMBASE_HALF_LIBRARY
    ILMBASE_IEX_LIBRARY
    ILMBASE_ILMTHREAD_LIBRARY
    ILMBASE_IMATH_LIBRARY
    ZLIB_FOUND)

mark_as_advanced(FORCE
    OPENEXR_ILMIMF_LIBRARY
    OPENEXR_INCLUDE_DIR
    ILMBASE_HALF_LIBRARY
    ILMBASE_IEX_LIBRARY
    ILMBASE_ILMTHREAD_LIBRARY
    ILMBASE_IMATH_LIBRARY)

# Dependency treee reverse-engineered from the 2.5 config files. They also have
# OpenEXR::IlmImfConfig / IlmBase::IlmBaseConfig which do just an include path
# setup, I find that unnecessary, the include path gets added to the Half / Iex
# libraries instead, which are needed by everything else.

if(NOT TARGET IlmBase::Half)
    add_library(IlmBase::Half UNKNOWN IMPORTED)
    set_target_properties(IlmBase::Half PROPERTIES
        IMPORTED_LOCATION ${ILMBASE_HALF_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENEXR_INCLUDE_DIR})
endif()

if(NOT TARGET IlmBase::Iex)
    add_library(IlmBase::Iex UNKNOWN IMPORTED)
    set_target_properties(IlmBase::Iex PROPERTIES
        IMPORTED_LOCATION ${ILMBASE_IEX_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENEXR_INCLUDE_DIR})
endif()

if(NOT TARGET IlmBase::IlmThread)
    add_library(IlmBase::IlmThread UNKNOWN IMPORTED)
    set_target_properties(IlmBase::IlmThread PROPERTIES
        IMPORTED_LOCATION ${ILMBASE_ILMTHREAD_LIBRARY}
        INTERFACE_LINK_LIBRARIES "IlmBase::Iex;Threads::Threads")
endif()

if(NOT TARGET IlmBase::Imath)
    add_library(IlmBase::Imath UNKNOWN IMPORTED)
    set_target_properties(IlmBase::Imath PROPERTIES
        IMPORTED_LOCATION ${ILMBASE_IMATH_LIBRARY}
        # The 2.5 config links to IexMath, which then transitively links to
        # Iex, but ldd has a different opinion -- the IexMath isn't needed at
        # all, it seems, so skip it.
        INTERFACE_LINK_LIBRARIES "IlmBase::Half;IlmBase::Iex")
endif()

if(NOT TARGET OpenEXR::IlmImf)
    add_library(OpenEXR::IlmImf UNKNOWN IMPORTED)
    set_target_properties(OpenEXR::IlmImf PROPERTIES
        IMPORTED_LOCATION ${OPENEXR_ILMIMF_LIBRARY}
        INTERFACE_LINK_LIBRARIES "IlmBase::Half;IlmBase::Iex;IlmBase::IlmThread;IlmBase::Imath;ZLIB::ZLIB")
endif()

# OpenEXR 3.0 compatible alias for versions 2.5 and below
if(NOT TARGET OpenEXR::OpenEXR)
    add_library(OpenEXR::OpenEXR INTERFACE IMPORTED)
    set_target_properties(OpenEXR::OpenEXR PROPERTIES
        INTERFACE_LINK_LIBRARIES OpenEXR::IlmImf)
endif()
