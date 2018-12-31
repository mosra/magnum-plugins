#.rst:
# Find FFmpeg
# -------------
#
# Finds the FFmpeg library. Basic usage:
#
#  find_package(FFmpeg REQUIRED avformat avcodec)
#
# This module defines:
#
#  FFmpeg_FOUND             - True if FFmpeg is found
#  FFmpeg_<component>_FOUND - True if given FFmpeg compoment is found
#  FFmpeg::ffmpeg           - Imported target for the ffmpeg executable
#  FFmpeg::<component>      - Imported target for given library component
#
# The <component> is any of these:
#
#  avcodec
#  avdevice
#  avfilter
#  avformat
#  avresample
#  avutil
#  swresample
#  swscale
#
# Additionally these variables are defined for internal usage:
#
#  FFmpeg_<component>_LIBRARY       - FFmpeg component library location
#  FFmpeg_<component>_INCLUDE_DIR   - FFmpeg component Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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

find_program(FFmpeg_EXECUTABLE ffmpeg)
if(FFmpeg_EXECUTABLE)
    add_executable(FFmpeg::ffmpeg IMPORTED)
    mark_as_advanced(FFmpeg_EXECUTABLE)
    set_property(TARGET FFmpeg::ffmpeg PROPERTY IMPORTED_LOCATION ${FFmpeg_EXECUTABLE})
endif()

foreach(_component ${FFmpeg_FIND_COMPONENTS})
    find_library(FFmpeg_${_component}_LIBRARY NAMES ${_component})
    find_path(FFmpeg_${_component}_INCLUDE_DIR NAMES lib${_component}/${_component}.h)

    if(FFmpeg_${_component}_LIBRARY AND FFmpeg_${_component}_INCLUDE_DIR)
        set(FFmpeg_${_component}_FOUND ON)

        if(NOT TARGET FFmpeg::${_component})
            add_library(FFmpeg::${_component} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${_component} PROPERTIES
                IMPORTED_LOCATION ${FFmpeg_${_component}_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${FFmpeg_${_component}_INCLUDE_DIR})
        endif()
    endif()

    mark_as_advanced(
        FFmpeg_${_component}_LIBRARY
        FFmpeg_${_component}_INCLUDE_DIR)
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg REQUIRED_VARS FFmpeg_EXECUTABLE HANDLE_COMPONENTS)
