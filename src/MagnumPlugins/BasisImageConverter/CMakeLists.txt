#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2019 Jonathan Hale <squareys@googlemail.com>
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

# C is needed on < 3.9 by TestBigEndian, which is used by FindBasisUniversal
if(CMAKE_VERSION VERSION_LESS 3.9)
    enable_language(C)
endif()

# To help Homebrew and Vcpkg packages, Basis Universal sources can be cloned to
# src/external and we will use those without any extra effort from the outside.
# Note that, because I hate underscores, the name has to contain a dash, *not*
# an underscore (which is how the GitHub repo is named)
if(NOT BASIS_UNIVERSAL_DIR AND EXISTS ${PROJECT_SOURCE_DIR}/src/external/basis-universal)
    set(BASIS_UNIVERSAL_DIR ${PROJECT_SOURCE_DIR}/src/external/basis-universal)
endif()

find_package(Magnum REQUIRED Trade)
find_package(BasisUniversal REQUIRED Encoder)

if(MAGNUM_BUILD_PLUGINS_STATIC AND NOT DEFINED MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC)
    set(MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

# BasisImageConverter plugin
add_plugin(BasisImageConverter
    imageconverters
    "${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_LIBRARY_INSTALL_DIR}"
    BasisImageConverter.conf
    BasisImageConverter.cpp
    BasisImageConverter.h)
if(MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC AND MAGNUM_BUILD_STATIC_PIC)
    set_target_properties(BasisImageConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(BasisImageConverter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(BasisImageConverter
    PUBLIC Magnum::Trade
    PRIVATE BasisUniversal::Encoder)

install(FILES BasisImageConverter.h ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/BasisImageConverter)

# Automatic static plugin import
if(MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/BasisImageConverter)
    target_sources(BasisImageConverter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
endif()

if(MAGNUM_BUILD_TESTS)
    add_subdirectory(Test ${EXCLUDE_FROM_ALL_IF_TEST_TARGET})
endif()

# MagnumPlugins BasisImageConverter target alias for superprojects
add_library(MagnumPlugins::BasisImageConverter ALIAS BasisImageConverter)
