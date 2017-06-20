#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(STANFORDIMPORTER_TEST_DIR ".")
else()
    set(STANFORDIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

corrade_add_test(StanfordImporterTest Test.cpp
    LIBRARIES MagnumStanfordImporterTestLib
    FILES
        big-endian.ply
        common.ply
        crlf.ply
        empty.ply
        ignored-vertex-components.ply
        incomplete-face.ply
        incomplete-vertex.ply
        invalid-face-index-type.ply
        invalid-face-size.ply
        invalid-face-size-type.ply
        invalid-format.ply
        invalid-signature.ply
        invalid-vertex-property.ply
        invalid-vertex-type.ply
        missing-format.ply
        short-file.ply
        unexpected-property.ply
        unknown-element.ply
        unknown-face-property.ply
        unknown-line.ply
        unsupported-format.ply)
target_include_directories(StanfordImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
# On Win32 we need to avoid dllimporting StanfordImporter symbols, because it
# would search for the symbols in some DLL even when they were linked
# statically. However it apparently doesn't matter that they were dllexported
# when building the static library. EH.
if(WIN32)
    target_compile_definitions(StanfordImporterTest PRIVATE "MAGNUM_STANFORDIMPORTER_BUILD_STATIC")
endif()
set_target_properties(StanfordImporterTest PROPERTIES FOLDER "MagnumPlugins/StanfordImporter/Test")
