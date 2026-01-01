#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025, 2026
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

set(MagnumPlugins_MODULES
    FindMagnumPlugins.cmake
    MagnumPluginsConfig.cmake)

# IMPORTANT: When adding a new module here, be sure to update the
# find_path(_MAGNUMPLUGINS_DEPENDENCY_MODULE_DIR ...) list in
# FindMagnumPlugins.cmake to avoid breakages when the directory contains only
# that new module.
set(MagnumPlugins_DEPENDENCY_MODULES )
if(MAGNUM_WITH_ASSIMPIMPORTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindAssimp.cmake)
endif()
if(MAGNUM_WITH_BASISIMPORTER OR MAGNUM_WITH_BASISIMAGECONVERTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        # FindBasisUniversal only needed for compiling the plugins themselves
        FindZstd.cmake)
endif()
if(MAGNUM_WITH_DEVILIMAGEIMPORTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindDevIL.cmake)
endif()
if(MAGNUM_WITH_GLSLANGSHADERCONVERTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindGlslang.cmake)
endif()
if(MAGNUM_WITH_HARFBUZZFONT)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindHarfBuzz.cmake)
endif()
if(MAGNUM_WITH_OPENEXRIMPORTER OR MAGNUM_WITH_OPENEXRIMAGECONVERTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindOpenEXR.cmake)
endif()
if(MAGNUM_WITH_RESVGIMPORTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindResvg.cmake)
endif()
if(MAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindSpirvTools.cmake)
endif()
if(MAGNUM_WITH_SPNGIMPORTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindSpng.cmake)
endif()
if(MAGNUM_WITH_WEBPIMPORTER OR MAGNUM_WITH_WEBPIMAGECONVERTER)
    list(APPEND MagnumPlugins_DEPENDENCY_MODULES
        FindWebP.cmake)
endif()

install(FILES ${MagnumPlugins_MODULES} DESTINATION ${MAGNUMPLUGINS_CMAKE_MODULE_INSTALL_DIR})
if(MagnumPlugins_DEPENDENCY_MODULES)
    install(FILES ${MagnumPlugins_DEPENDENCY_MODULES} DESTINATION ${MAGNUMPLUGINS_CMAKE_MODULE_INSTALL_DIR}/dependencies)
endif()

# Magnum Plugins dependency module dir for superprojects
set(_MAGNUMPLUGINS_DEPENDENCY_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")
