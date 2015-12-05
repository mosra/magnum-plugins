#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2015 Jonathan Hale <squareys@googlemail.com>
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

find_package(Magnum REQUIRED Audio)

include_directories(${MAGNUM_AUDIO_INCLUDE_DIRS})
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/src/external/stb)

set(StbVorbisAudioImporter_SRCS
    StbVorbisImporter.cpp)

set(StbVorbisAudioImporter_HEADERS
    StbVorbisImporter.h)

# Objects shared between plugin and test library
add_library(StbVorbisAudioImporterObjects OBJECT
    ${StbVorbisAudioImporter_SRCS}
    ${StbVorbisAudioImporter_HEADERS})
if(NOT BUILD_PLUGINS_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbVorbisAudioImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# StbVorbisImporter plugin
add_plugin(StbVorbisAudioImporter ${MAGNUM_PLUGINS_AUDIOIMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_AUDIOIMPORTER_RELEASE_INSTALL_DIR}
    StbVorbisAudioImporter.conf
    $<TARGET_OBJECTS:StbVorbisAudioImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbVorbisAudioImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(StbVorbisAudioImporter ${MAGNUM_LIBRARIES} ${MAGNUM_AUDIO_LIBRARIES})

install(FILES ${StbVorbisAudioImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbVorbisAudioImporter)

if(BUILD_TESTS)
    add_library(MagnumStbVorbisAudioImporterTestLib STATIC $<TARGET_OBJECTS:StbVorbisAudioImporterObjects>)
    target_link_libraries(MagnumStbVorbisAudioImporterTestLib ${MAGNUM_LIBRARIES} ${MAGNUM_AUDIO_LIBRARIES})
    add_subdirectory(Test)
endif()

# Magnum StbVorbisAudioImporter library for superprojects
set(MAGNUMPLUGINS_STBVORBISAUDIOIMPORTER_LIBRARY StbVorbisAudioImporter CACHE INTERNAL "")
