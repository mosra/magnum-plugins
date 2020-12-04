#.rst:
# Find Assimp
# -------------
#
# Finds the Assimp library. This module defines:
#
#  Assimp_FOUND           - True if Assimp library is found
#  Assimp::Assimp         - Assimp imported target
#
# Additionally these variables are defined for internal usage:
#
#  Assimp_LIBRARY         - Assimp library
#  Assimp_INCLUDE_DIR     - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2017 Jonathan Hale <squareys@googlemail.com>
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

# Assimp installs a config file that can give us all its dependencies in case
# of a static build. In case the assimp target is already present, we have it
# as a CMake subproject, so don't attempt to look for it again.
if(NOT TARGET assimp)
    # Assimp's own config file contains if(ON) and because its CMake
    # requirement is set to an insanely low version 2.6, CMake complains about
    # a policy change. To suppress it, neither cmake_policy(SET CMP0012 NEW)
    # nor find_package(... NO_POLICY_SCOPE) does anything, fortunately there's
    # this variable that's able to punch through the scope created by
    # find_package(): https://cmake.org/cmake/help/latest/variable/CMAKE_POLICY_DEFAULT_CMPNNNN.html
    set(CMAKE_POLICY_DEFAULT_CMP0012 NEW)
    find_package(assimp CONFIG QUIET)
    unset(CMAKE_POLICY_DEFAULT_CMP0012)

    # Vanilla Assimp config files are completely broken because they don't set
    # IMPORTED_CONFIGURATIONS on anything except MSVC. Detect that case and
    # then skip aliasing the target to assimp::assimp because it will warn
    # about
    #   IMPORTED_LOCATION not set for imported target "assimp::assimp"
    # and subsequently fail with
    #   ninja: error: 'assimp::assimp-NOTFOUND', needed by '<target>', missing
    #   and no known rule to make it
    # This should be fixed with https://github.com/assimp/assimp/pull/3455 and
    # is also fixed in vcpkg, which creates its own non-broken config files:
    # https://github.com/microsoft/vcpkg/pull/14554
    #
    # In addition, old config files (Assimp 3.2, i.e.) don't define any target
    # at all, so don't event attempt to query anything there.
    if(assimp_FOUND AND TARGET assimp::assimp)
        get_target_property(_ASSIMP_IMPORTED_CONFIGURATIONS assimp::assimp IMPORTED_CONFIGURATIONS)
        if(NOT _ASSIMP_IMPORTED_CONFIGURATIONS)
            set(_ASSIMP_HAS_BROKEN_IMPORTED_TARGET ON)
        endif()
    endif()
endif()

# In case we have Assimp as a CMake subproject or the config file was present,
# simply alias our target to that. The assimp config file is actually
# assimp::assimp and while the CMake subproject defines assimp::assimp as well,
# it's like that only since version 5:
#   https://github.com/assimp/assimp/commit/b43cf9233703305cfd8dfe7844fce959879b4f0c
#   https://github.com/assimp/assimp/commit/30d3c8c6a37a3b098702dfb714fe8e5e2abbfa5e
# The target aliasing is skipped in case the config files are broken, see above
if((TARGET assimp OR TARGET assimp::assimp) AND NOT _ASSIMP_HAS_BROKEN_IMPORTED_TARGET)
    if(TARGET assimp)
        set(_ASSIMP_TARGET assimp)
    else()
        set(_ASSIMP_TARGET assimp::assimp)
    endif()

    get_target_property(_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES ${_ASSIMP_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    # In case of a CMake subproject (which always has the assimp target, not
    # assimp::assimp, so we don't need to use ${_ASSIMP_TARGET}), the target
    # doesn't define any usable INTERFACE_INCLUDE_DIRECTORIES for some reason
    # (the $<BUILD_INTERFACE:> in there doesn't get expanded), so let's extract
    # that from the SOURCE_DIR property instead.
    if(_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES MATCHES "<BUILD_INTERFACE:")
        get_target_property(_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES assimp SOURCE_DIR)
        get_filename_component(_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES ${_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
        set(_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES ${_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES}/include)
    endif()

    if(NOT TARGET Assimp::Assimp)
        # Aliases of (global) targets are only supported in CMake 3.11, so we
        # work around it by this. This is easier than fetching all possible
        # properties (which are impossible to track of) and then attempting to
        # rebuild them into a new target.
        add_library(Assimp::Assimp INTERFACE IMPORTED)
        set_target_properties(Assimp::Assimp PROPERTIES
            INTERFACE_LINK_LIBRARIES ${_ASSIMP_TARGET})
        set_target_properties(Assimp::Assimp PROPERTIES
            # Needs to be wrapped in quotes because the shit Assimp config file
            # sets it to "${_IMPORT_PREFIX}/include;${_IMPORT_PREFIX}/include"
            # (twice the same dir) for some reason.
            INTERFACE_INCLUDE_DIRECTORIES "${_ASSIMP_INTERFACE_INCLUDE_DIRECTORIES}")
    endif()

    # Just to make FPHSA print some meaningful location, nothing else
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args("Assimp" DEFAULT_MSG
        _ASSIMP_INTERFACE_INCLUDE_DIRECTORIES)

    return()
endif()

find_path(Assimp_INCLUDE_DIR NAMES assimp/anim.h
    HINTS
        include
        # For backwards compatibility
        ${ASSIMP_INCLUDE_DIR})

if(WIN32 AND MSVC)
    # Adapted from https://github.com/assimp/assimp/blob/799fd74714f9ffac29004c6b5a674b3402524094/CMakeLists.txt#L645-L655
    # with versions below MSVC 2015 (14.0 / 1900) removed, and the discouraged
    # use of MSVCxy replaced with MSVC_VERSION. See also
    # https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B#Internal_version_numbering
    if(MSVC_TOOLSET_VERSION) # available only since CMake 3.12
        set(Assimp_MSVC_VERSION vc${MSVC_TOOLSET_VERSION})
    elseif(MSVC_VERSION VERSION_LESS 1910)
        set(Assimp_MSVC_VERSION vc140)
    elseif(MSVC_VERSION VERSION_LESS 1920)
        set(Assimp_MSVC_VERSION vc141)
    elseif(MSVC_VERSION VERSION_LESS 1930)
        set(Assimp_MSVC_VERSION vc142)
    else()
        message(FATAL_ERROR "Unsupported MSVC version")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(Assimp_LIBRARY_DIR "lib64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(Assimp_LIBRARY_DIR "lib32")
    endif()

    find_library(Assimp_LIBRARY_RELEASE assimp-${Assimp_MSVC_VERSION}-mt.lib
        PATHS ${Assimp_LIBRARY_DIR}
        # For backwards compatibility
        HINTS ${ASSIMP_LIBRARY_RELEASE})
    find_library(Assimp_LIBRARY_DEBUG assimp-${Assimp_MSVC_VERSION}-mtd.lib
        PATHS ${Assimp_LIBRARY_DIR}
        # For backwards compatibility
        HINTS ${ASSIMP_LIBRARY_DEBUG})
else()
    find_library(Assimp_LIBRARY_RELEASE assimp)
    find_library(Assimp_LIBRARY_DEBUG assimpd)
endif()

# Static build of Assimp (built with Vcpkg, any system) depends on IrrXML, find
# that one as well. If not found, simply don't link to it --- it might be a
# dynamic build (on Windows it's a *.lib either way), or a static build using
# system IrrXML. Related issue: https://github.com/Microsoft/vcpkg/issues/5012
if(Assimp_LIBRARY_DEBUG MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$" OR Assimp_LIBRARY_RELEASE MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    find_library(Assimp_IRRXML_LIBRARY_RELEASE IrrXML)
    find_library(Assimp_IRRXML_LIBRARY_DEBUG IrrXMLd)
endif()

include(SelectLibraryConfigurations)
select_library_configurations(Assimp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Assimp DEFAULT_MSG
    Assimp_LIBRARY
    Assimp_INCLUDE_DIR)

if(NOT TARGET Assimp::Assimp)
    add_library(Assimp::Assimp UNKNOWN IMPORTED)

    if(Assimp_LIBRARY_DEBUG)
        set_property(TARGET Assimp::Assimp APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Assimp::Assimp PROPERTIES IMPORTED_LOCATION_DEBUG ${Assimp_LIBRARY_DEBUG})
    endif()
    if(Assimp_LIBRARY_RELEASE)
        set_property(TARGET Assimp::Assimp APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Assimp::Assimp PROPERTIES IMPORTED_LOCATION_RELEASE ${Assimp_LIBRARY_RELEASE})
    endif()

    # Link to IrrXML as well, if found. See the comment above for details.
    if(Assimp_IRRXML_LIBRARY_RELEASE)
        set_property(TARGET Assimp::Assimp APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES $<$<NOT:$<CONFIG:Debug>>:${Assimp_IRRXML_LIBRARY_RELEASE}>)
    endif()
    if(Assimp_IRRXML_LIBRARY_DEBUG)
        set_property(TARGET Assimp::Assimp APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES $<$<CONFIG:Debug>:${Assimp_IRRXML_LIBRARY_DEBUG}>)
    endif()

    set_target_properties(Assimp::Assimp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${Assimp_INCLUDE_DIR})
endif()
