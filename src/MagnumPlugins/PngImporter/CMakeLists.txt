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

find_package(PNG REQUIRED)

include_directories(${PNG_INCLUDE_DIRS})

set(PngImporter_SRCS
    PngImporter.cpp)

set(PngImporter_HEADERS
    PngImporter.h)

add_library(PngImporterObjects OBJECT
    ${PngImporter_SRCS}
    ${PngImporter_HEADERS})
set_target_properties(PngImporterObjects PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} -DPngImporterObjects_EXPORTS")

add_plugin(PngImporter ${MAGNUM_PLUGINS_IMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMPORTER_RELEASE_INSTALL_DIR}
    PngImporter.conf
    $<TARGET_OBJECTS:PngImporterObjects>
    pluginRegistration.cpp)
target_link_libraries(PngImporter
    ${MAGNUM_LIBRARIES}
    ${PNG_LIBRARIES})

install(FILES ${PngImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/PngImporter)

if(BUILD_TESTS)
    add_library(MagnumPngImporterTestLib SHARED $<TARGET_OBJECTS:PngImporterObjects>)
    set_target_properties(MagnumPngImporterTestLib PROPERTIES DEBUG_POSTFIX "-d")
    target_link_libraries(MagnumPngImporterTestLib
        ${MAGNUM_LIBRARIES}
        ${PNG_LIBRARIES})

    # On Windows we need to install first and then run the tests to avoid "DLL
    # not found" hell, thus we need to install this too
    if(CORRADE_TARGET_WINDOWS AND NOT CMAKE_CROSSCOMPILING)
        install(TARGETS MagnumPngImporterTestLib
            RUNTIME DESTINATION ${MAGNUM_BINARY_INSTALL_DIR}
            LIBRARY DESTINATION ${MAGNUM_LIBRARY_INSTALL_DIR}
            ARCHIVE DESTINATION ${MAGNUM_LIBRARY_INSTALL_DIR})
    endif()

    add_subdirectory(Test)
endif()
