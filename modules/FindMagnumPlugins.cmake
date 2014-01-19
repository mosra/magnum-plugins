# - Find Magnum plugins
#
# Basic usage:
#  find_package(MagnumPlugins [REQUIRED])
# This command tries to find Magnum plugins and then defines:
#  MAGNUMPLUGINS_FOUND          - Whether Magnum plugins were found
# This command will not try to find any actual plugin. The plugins are:
#  ColladaImporter  - Collada importer (depends on Qt library)
#  FreeTypeFont     - FreeType font (depends on FreeType library)
#  HarfBuzzFont     - HarfBuzz font (depends on FreeType plugin and HarfBuzz
#                     library)
#  JpegImporter     - JPEG importer (depends on libJPEG library)
#  PngImporter      - PNG importer (depends on libPNG library)
# Example usage with specifying the plugins is:
#  find_package(MagnumPlugins [REQUIRED|COMPONENTS]
#               MagnumFont TgaImporter)
# For each plugin is then defined:
#  MAGNUMPLUGINS_*_FOUND        - Whether the plugin was found
#  MAGNUMPLUGINS_*_LIBRARIES    - Plugin library and dependent libraries
#  MAGNUMPLUGINS_*_INCLUDE_DIRS - Include dirs of plugin dependencies
#
# If `MAGNUM_BUILD_DEPRECATED` is defined, `MAGNUM_PLUGINS_INCLUDE_DIRS`
# contains include dir for plugins (i.e. instead of `MagnumPlugins/` prefix)
# and include dirs of dependencies.
#
# Additionally these variables are defined for internal usage:
#  MAGNUMPLUGINS_*_LIBRARY      - Plugin library (w/o dependencies)
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014
#             Vladimír Vondruš <mosra@centrum.cz>
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

# Dependencies
find_package(Magnum REQUIRED)

# Additional components
foreach(component ${MagnumPlugins_FIND_COMPONENTS})
    string(TOUPPER ${component} _COMPONENT)

    # Plugin library suffix
    if(${component} MATCHES ".+AudioImporter$")
        set(_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX audioimporters)
    elseif(${component} MATCHES ".+Importer$")
        set(_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX importers)
    elseif(${component} MATCHES ".+Font$")
        set(_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX fonts)
    elseif(${component} MATCHES ".+ImageConverter$")
        set(_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX imageconverters)
    elseif(${component} MATCHES ".+FontConverter$")
        set(_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX fontconverters)
    endif()

    # Find the library. Dynamic plugins don't have any prefix (e.g. `lib` on
    # Linux), search with empty prefix and then reset that back so we don't
    # accidentaly break something else
    set(_tmp_prefixes ${CMAKE_FIND_LIBRARY_PREFIXES})
    set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_FIND_LIBRARY_PREFIXES} "")
    find_library(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY ${component}
        PATH_SUFFIXES magnum/${_MAGNUMPLUGINS_${_COMPONENT}_PATH_SUFFIX})
    set(CMAKE_FIND_LIBRARY_PREFIXES ${_tmp_prefixes})

    # Find include path
    find_path(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIR
            NAMES ${component}.h
            PATHS ${MAGNUM_INCLUDE_DIR}/MagnumPlugins/${component})

    # ColladaImporter plugin dependencies
    if(${component} STREQUAL ColladaImporter)
        find_package(Qt4)
        if(QT4_FOUND)
            set(_MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY})
            set(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${QT_INCLUDE_DIR})
        else()
            unset(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY)
        endif()
    endif()

    # FreeTypeFont plugin dependencies
    if(${component} STREQUAL FreeTypeFont)
        find_package(Freetype)
        if(FREETYPE_FOUND)
            set(_MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${FREETYPE_LIBRARIES})
            set(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})
        else()
            unset(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY)
        endif()
    endif()

    # HarfBuzzFont plugin dependencies
    if(${component} STREQUAL HarfBuzzFont)
        find_package(Freetype)
        find_package(HarfBuzz)
        if(FREETYPE_FOUND AND HARFBUZZ_FOUND)
            set(_MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${FREETYPE_LIBRARIES} ${HARFBUZZ_LIBRARIES})
            set(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS} ${HARFBUZZ_INCLUDE_DIRS})
        else()
            unset(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY)
        endif()
    endif()

    # JpegImporter plugin dependencies
    if(${component} STREQUAL JpegImporter)
        find_package(JPEG)
        if(JPEG_FOUND)
            set(_MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${JPEG_LIBRARIES})
            set(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${JPEG_INCLUDE_DIR})
        else()
            unset(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY)
        endif()
    endif()

    # PngImporter plugin dependencies
    if(${component} STREQUAL PngImporter)
        find_package(PNG)
        if(PNG_FOUND)
            set(_MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${PNG_LIBRARIES})
            set(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${PNG_INCLUDE_DIRS})
        else()
            unset(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY)
        endif()
    endif()

    # Decide if the plugin was found
    if(MAGNUMPLUGINS_${_COMPONENT}_LIBRARY AND _MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIR)
        set(MAGNUMPLUGINS_${_COMPONENT}_LIBRARIES ${MAGNUMPLUGINS_${_COMPONENT}_LIBRARY} ${_MAGNUM_${_COMPONENT}_LIBRARIES})
        set(MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS ${_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIRS})

        set(MagnumPlugins_${component}_FOUND TRUE)

        # Don't expose variables w/o dependencies to end users
        mark_as_advanced(FORCE MAGNUMPLUGINS_${_COMPONENT}_LIBRARY _MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIR)
    else()
        set(MagnumPlugins_${component}_FOUND FALSE)
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MagnumPlugins
    REQUIRED_VARS MAGNUMPLUGINS_INCLUDE_DIR
    HANDLE_COMPONENTS)

# Dependent libraries and includes
set(MAGNUMPLUGINS_INCLUDE_DIRS ${MAGNUM_INCLUDE_DIRS} ${MAGNUM_INCLUDE_DIR}/MagnumPlugins)
mark_as_advanced(FORCE MAGNUMPLUGINS_INCLUDE_DIR)
