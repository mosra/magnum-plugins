#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020 Vladimír Vondruš <mosra@centrum.cz>
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

find_package(Magnum REQUIRED ShaderTools)

# See modules/FindSpirvTools.cmake for the complete rant
find_package(SpirvTools REQUIRED)

if(BUILD_PLUGINS_STATIC)
    set(MAGNUM_SPIRVTOOLSSHADERCONVERTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

# It's 2020 so nothing surprises me anymore -- of course SPIRV-Tools don't have
# any reasonable way to know the version except for parsing the CHANGES file,
# which might or might not be present. Thus we have to iterate over all
# versions of interest and pick the highest one that compiles.
foreach(_version 202001 201905 201904 201903 201902)
    try_compile(_works ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/checkSpirvToolsVersion.cpp
        LINK_LIBRARIES SpirvTools::SpirvTools SpirvTools::Opt
        COMPILE_DEFINITIONS
            -DCHECK_VERSION=${_version}
            # I thought I could make this use C++11 by linking to
            # Corrade::Utility, but for some reason the effect of the
            # CORRADE_CXX_STANDARD property doesn't get passed through. So I
            # have to use an internal variable for that instead.
            ${CORRADE_CXX11_STANDARD_FLAG}
        OUTPUT_VARIABLE _version_output)
    if(_works)
        set(SPIRVTOOLS_VERSION ${_version})
        break()
    endif()
endforeach()
if(NOT SPIRVTOOLS_VERSION)
    message(SEND_ERROR "Could not detect SPIRV-Tools version. Maybe even older than 2019.2? ${_version_output}")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configureInternal.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configureInternal.h)

# SpirvToolsShaderConverter plugin
add_plugin(SpirvToolsShaderConverter
    "${MAGNUM_PLUGINS_SHADERCONVERTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_SHADERCONVERTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_SHADERCONVERTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_SHADERCONVERTER_RELEASE_LIBRARY_INSTALL_DIR}"
    SpirvToolsShaderConverter.conf
    SpirvToolsConverter.cpp
    SpirvToolsConverter.h)
if(BUILD_PLUGINS_STATIC AND BUILD_STATIC_PIC)
    set_target_properties(SpirvToolsShaderConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(SpirvToolsShaderConverter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(SpirvToolsShaderConverter PUBLIC
    Magnum::ShaderTools
    SpirvTools::SpirvTools
    SpirvTools::Opt)
# Modify output location only if all are set, otherwise it makes no sense
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY AND CMAKE_LIBRARY_OUTPUT_DIRECTORY AND CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set_target_properties(SpirvToolsShaderConverter PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/shaderconverters
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/shaderconverters
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/shaderconverters)
endif()

install(FILES SpirvToolsConverter.h ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/SpirvToolsShaderConverter)

# Automatic static plugin import
if(BUILD_PLUGINS_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/SpirvToolsShaderConverter)
    target_sources(SpirvToolsShaderConverter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
endif()

if(BUILD_TESTS)
    add_subdirectory(Test)
endif()

# MagnumPlugins SpirvToolsShaderConverter target alias for superprojects
add_library(MagnumPlugins::SpirvToolsShaderConverter ALIAS SpirvToolsShaderConverter)
