#.rst:
# Find Glslang
# ------------
#
# Finds the glslang library. This module defines:
#
#  Glslang_FOUND        - True if the Glslang library is found
#  Glslang::Glslang     - Glslang imported target
#  Glslang::SPIRV       - Glslang libSPIRV imported target
#  Glslang::HLSL        - Glslang libHLSL imported target, if found
#  Glslang::OSDependent - Glslang libOSDependent imported target, if found
#
# Link to Glslang::Glslang, all its dependencies will get linked transitively.
# Additionally these variables are defined for internal usage:
#
#  GLSLANG_LIBRARY_{DEBUG,RELEASE} - Glslang library
#  GLSLANG_SPIRV_LIBRARY_{DEBUG,RELEASE} - Glslang libSPIRV library
#  GLSLANG_HLSL_LIBRARY_{DEBUG,RELEASE} - Glslang libHLSL library
#  GLSLANG_OSDEPENDENT_LIBRARY_{DEBUG,RELEASE} - Glslang libOSDependent library
#  GLSLANG_INCLUDE_DIR  - Include dir
#
# Actually, it's 2020 and none of this should be needed, but apparently Magnum
# is the first project ever to use this thing from Khronos as an actual library
# instead of the CLI tool. Their CMake system installs a ton of
# ``glslangTargets.cmake`` files, but no actual ``glslangConfig.cmake``, so I
# have to look for it myself. Scroll below for a continuation of this angry
# rant.
#

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

# Libraries. The debug suffix is used only on Windows.
find_library(GLSLANG_LIBRARY_RELEASE NAMES glslang)
find_library(GLSLANG_LIBRARY_DEBUG NAMES glslangd)
find_library(GLSLANG_SPIRV_LIBRARY_RELEASE NAMES SPIRV)
find_library(GLSLANG_SPIRV_LIBRARY_DEBUG NAMES SPIRVd)
# These are needed only in a static build. And because it's impossible to
# detect static build on Windows, we'll just treat them as optional.
find_library(GLSLANG_HLSL_LIBRARY_RELEASE NAMES HLSL)
find_library(GLSLANG_HLSL_LIBRARY_DEBUG NAMES HLSLd)
# Are you mad?! Why the fuck can't you put everything into a single lib FFS
find_library(GLSLANG_OSDEPENDENT_LIBRARY_RELEASE NAMES OSDependent)
find_library(GLSLANG_OSDEPENDENT_LIBRARY_DEBUG NAMES OSDependentd)
find_library(GLSLANG_OGLCOMPILER_LIBRARY_RELEASE NAMES OGLCompiler)
find_library(GLSLANG_OGLCOMPILER_LIBRARY_DEBUG NAMES OGLCompilerd)

include(SelectLibraryConfigurations)
select_library_configurations(GLSLANG)
select_library_configurations(GLSLANG_SPIRV)
select_library_configurations(GLSLANG_HLSL)
select_library_configurations(GLSLANG_OSDEPENDENT)
select_library_configurations(GLSLANG_OGLCOMPILER)

# Include dir
find_path(GLSLANG_INCLUDE_DIR
    # Actually, WHAT THE FUCK, I get that some people suck at naming, but this
    # is an actual naming skill black hole. Even naming it Windows.h would make
    # more sense than this. Like, what the hell.
    NAMES glslang/Public/ShaderLang.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glslang DEFAULT_MSG
    GLSLANG_LIBRARY
    GLSLANG_SPIRV_LIBRARY
    GLSLANG_INCLUDE_DIR)

mark_as_advanced(FORCE GLSLANG_INCLUDE_DIR)

if(NOT TARGET Glslang::SPIRV)
    add_library(Glslang::SPIRV UNKNOWN IMPORTED)
    if(GLSLANG_SPIRV_LIBRARY_RELEASE)
        set_property(TARGET Glslang::SPIRV APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::SPIRV PROPERTIES
            IMPORTED_LOCATION_RELEASE ${GLSLANG_SPIRV_LIBRARY_RELEASE})
    endif()
    if(GLSLANG_SPIRV_LIBRARY_DEBUG)
        set_property(TARGET Glslang::SPIRV APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::SPIRV PROPERTIES
            IMPORTED_LOCATION_DEBUG ${GLSLANG_SPIRV_LIBRARY_DEBUG})
    endif()
endif()

# Optional because needed only in a static build, which is impossible to detect
if((GLSLANG_HLSL_LIBRARY_DEBUG OR GLSLANG_HLSL_LIBRARY_RELEASE) AND NOT TARGET Glslang::HLSL)
    add_library(Glslang::HLSL UNKNOWN IMPORTED)
    if(GLSLANG_HLSL_LIBRARY_RELEASE)
        set_property(TARGET Glslang::HLSL APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::HLSL PROPERTIES
            IMPORTED_LOCATION_RELEASE ${GLSLANG_HLSL_LIBRARY_RELEASE})
    endif()
    if(GLSLANG_HLSL_LIBRARY_DEBUG)
        set_property(TARGET Glslang::HLSL APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::HLSL PROPERTIES
            IMPORTED_LOCATION_DEBUG ${GLSLANG_HLSL_LIBRARY_DEBUG})
    endif()
endif()
if((GLSLANG_OSDEPENDENT_LIBRARY_DEBUG OR GLSLANG_OSDEPENDENT_LIBRARY_RELEASE) AND NOT TARGET Glslang::OSDependent)
    add_library(Glslang::OSDependent UNKNOWN IMPORTED)
    if(GLSLANG_OSDEPENDENT_LIBRARY_RELEASE)
        set_property(TARGET Glslang::OSDependent APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::OSDependent PROPERTIES
            IMPORTED_LOCATION_RELEASE ${GLSLANG_OSDEPENDENT_LIBRARY_RELEASE})
    endif()
    if(GLSLANG_OSDEPENDENT_LIBRARY_DEBUG)
        set_property(TARGET Glslang::OSDependent APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::OSDependent PROPERTIES
            IMPORTED_LOCATION_DEBUG ${GLSLANG_OSDEPENDENT_LIBRARY_DEBUG})
    endif()
endif()
if((GLSLANG_OGLCOMPILER_LIBRARY_DEBUG OR GLSLANG_OGLCOMPILER_LIBRARY_RELEASE) AND NOT TARGET Glslang::OGLCompiler)
    add_library(Glslang::OGLCompiler UNKNOWN IMPORTED)
    if(GLSLANG_OGLCOMPILER_LIBRARY_RELEASE)
        set_property(TARGET Glslang::OGLCompiler APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::OGLCompiler PROPERTIES
            IMPORTED_LOCATION_RELEASE ${GLSLANG_OGLCOMPILER_LIBRARY_RELEASE})
    endif()
    if(GLSLANG_OGLCOMPILER_LIBRARY_DEBUG)
        set_property(TARGET Glslang::OGLCompiler APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::OGLCompiler PROPERTIES
            IMPORTED_LOCATION_DEBUG ${GLSLANG_OGLCOMPILER_LIBRARY_DEBUG})
    endif()

    find_package(Threads REQUIRED)
    set_property(TARGET Glslang::OGLCompiler APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES Threads::Threads)
endif()

if(NOT TARGET Glslang::Glslang)
    add_library(Glslang::Glslang UNKNOWN IMPORTED)
    if(GLSLANG_LIBRARY_RELEASE)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::Glslang PROPERTIES
            IMPORTED_LOCATION_RELEASE ${GLSLANG_LIBRARY_RELEASE})
    endif()
    if(GLSLANG_LIBRARY_DEBUG)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::Glslang PROPERTIES
            IMPORTED_LOCATION_DEBUG ${GLSLANG_LIBRARY_DEBUG})
    endif()
    set_target_properties(Glslang::Glslang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            # Second entry to account for broken relative includes in version
            # 7.13. Sigh. https://github.com/KhronosGroup/glslang/issues/2007
            "${GLSLANG_INCLUDE_DIR};${GLSLANG_INCLUDE_DIR}/glslang"
        INTERFACE_LINK_LIBRARIES Glslang::SPIRV)
    # Optional because needed only in a static build, which is impossible to
    # detect
    if(TARGET Glslang::HLSL)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::HLSL)
    endif()
    if(TARGET Glslang::OSDependent)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OSDependent)
    endif()
    if(TARGET Glslang::OGLCompiler)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OGLCompiler)
    endif()
endif()
