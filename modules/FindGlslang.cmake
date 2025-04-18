#.rst:
# Find Glslang
# ------------
#
# Finds the glslang library. This module defines:
#
#  Glslang_FOUND        - True if the Glslang library is found
#  Glslang::Glslang     - Glslang imported target.
#   dependency of Glslang::Glslang.
#
# Link to Glslang::Glslang. The other dependencies, if needed, will get linked
# transitively.
#
# Additionally these variables are defined for internal usage:
#
#  Glslang_*_LIBRARY_{DEBUG,RELEASE} - Glslang libraries
#  Glslang_INCLUDE_DIR  - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025
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

# It's 2025 and none of this should be needed, but apparently Magnum is (still)
# the first project ever to use this thing from Khronos as an actual library
# instead of the CLI tool, or at least in a way that tries to make it work with
# more than just one version. Back in 2020, when the first version of this Find
# module was written, their CMake system installed a ton of
# `glslangTargets.cmake` files, but no actual `glslangConfig.cmake`. That got
# fixed in 11.11 (https://github.com/KhronosGroup/glslang/commit/fb64704060ffbd7048f25a07518c55639726c025,
# released August 2022), from there on there's both `glslangTargets.cmake` and
# `glslang/glslang-targets.cmake` (amazing, innit). HOWEVER, while the
# `glslang::glslang` target works, attempting to use `glslang::SPIRV` results
# in `<prefix>/include/External` being added into
# INTERFACE_INCLUDE_DIRECTORIES, which just doesn't exist, so the target as a
# whole is useless and one has to manually extract everything except the
# INTERFACE_INCLUDE_DIRECTORIES into a new target in order to make it work.
# That's fixed in version 14:
#  https://github.com/KhronosGroup/glslang/pull/3420
#
# To add even more salt into the wound, version 14.0 removed the HLSL and
# OGLCompiler libraries (https://github.com/KhronosGroup/glslang/commit/6be56e45e574b375d759b89dad35f780bbd4792f)
# which apparently were just stubs since 11.0 already, and OF FUCKING COURSE
# version 11.0 is neither a proper release on GitHub (it's just a tag), nor
# the CHANGES.md mention anything about these libraries being stubs since 11.0
# (it's a major new release, and all it mentions is removal of MSVC 2013
# support, HAHA). To account for this, the file-by-file finding code would need
# to parse some version header first, regexps and other nasty brittle stuff.
# Not nice at all.
#
# Then! More salt! With version 15.0, the SPIRV target, which used to link to
# glslang, is removed, and glslang is the top-level target instead. Which means
# I have to make one or the other the top-level target, which OF COURSE DOES
# NOT MAKE ANYTHING SIMPLER THAN BEFORE. And these people think it's about
# JUST REMOVING THE REFERENCE!!!! JUST!!!! Like if I could afford to care only
# about the last bug-ridden release that falls out of their hands, like if
# there would be no LTS distros from five years ago!!!
#  https://github.com/KhronosGroup/glslang/issues/3882#issuecomment-2692374048
# Do you see that comment?!
#
# Thus, aside of the mess with randomly appearing and disappearing libraries,
# both the variant with attempting to use the config file and the variant with
# attempting to not use the config file is a total nightmare, but I _think_ the
# former is more future-proof, even with the extra nastiness given by property
# extraction.
#
# Scroll further for a continuation of this angry rant.

# In case we have glslang as a CMake subproject, the glslang target should be
# defined. If it's not, try to find the installed config file (which exists
# only since 11.11, and is still semi-useless even in version 14, as described
# above).
if(NOT TARGET glslang)
    # CMake config files produced by glslang 12.1 link to Threads::Threads but
    # there's no corresponding find_package(Threads), leading to any use of the
    # glslang target dying with
    #
    #  The link interface of target "glslang::OSDependent" contains:
    #
    #    Threads::Threads
    #
    #  but the target was not found
    #
    # It's fixed in 12.2 with https://github.com/KhronosGroup/glslang/commit/1db9cd28549831d7b884a12dc700bf88e21fb4b8
    # and then subsequently patched in 14.1 with https://github.com/KhronosGroup/glslang/commit/82e0d00b32d2245efd9da8196f139bc0564765d1
    # so it's likely still broken for years to come. Finding the Threads
    # package before attempting to find the glslang module fixes that.
    find_package(Threads REQUIRED QUIET)

    find_package(glslang CONFIG QUIET)
endif()

# We have a CMake subproject or a package where the (semi-useless) config files
# exist and were installed, point Glslang::Glslang there and exit.
if(TARGET glslang OR TARGET glslang::glslang)
    if(TARGET glslang)
        set(_GLSLANG_TARGET_PREFIX )
    else()
        set(_GLSLANG_TARGET_PREFIX glslang::)
    endif()

    get_target_property(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES ${_GLSLANG_TARGET_PREFIX}glslang INTERFACE_INCLUDE_DIRECTORIES)
    # In case of a CMake subproject (which always has the glslang target, not
    # glslang::glslang, so we don't need to use ${_GLSLANG_TARGET_PREFIX}),
    # the target doesn't define any usable INTERFACE_INCLUDE_DIRECTORIES for
    # some reason (the $<BUILD_INTERFACE:> in there doesn't get expanded), so
    # let's extract that from the SOURCE_DIR property instead.
    #
    # TODO this could be probably fixable by using target_link_libraries()
    # instead of set_target_properties() because it evaluates generator
    # expressions, but that needs CMake 3.10+, before that
    # target_link_libraries() can't be called on INTERFACE targets.
    if(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES MATCHES "<BUILD_INTERFACE:")
        get_target_property(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES glslang SOURCE_DIR)
        get_filename_component(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES ${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
    endif()

    if(NOT TARGET Glslang::Glslang)
        # Aliases of (global) targets are only supported in CMake 3.11, so
        # we work around it by this. This is easier than fetching all
        # possible properties (which are impossible to track of) and then
        # attempting to rebuild them into a new target.
        add_library(Glslang::Glslang INTERFACE IMPORTED)
        # Add the include path. In case of a CMake subproject, it's the one
        # extracted from SOURCE_DIR above.
        if(TARGET glslang)
            set_target_properties(Glslang::Glslang PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES ${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES})
        # In case of an imported target, can't just transitively use the one
        # from glslang::glslang because the code needs to use
        # <SPIRV/GlslangToSpirv.h> instead of <glslang/SPIRV/GlslangToSpirv.h>
        # in order to work correctly both with the library installed and the
        # with the library being a subproject (the latter doesn't have SPIRV/
        # inside glslang/). We however also need the top-level directory for
        # <glslang/Public/ShaderLang.h> and such (yeah, WHAT THE FUCK this
        # name, I know).
        else()
            set_target_properties(Glslang::Glslang PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES};${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES}/glslang")
        endif()

        # If the SPIRV target exists (on versions 14 and older), that's the
        # top-level target to link to
        if((TARGET glslang AND TARGET SPIRV) OR (TARGET glslang::glslang AND TARGET glslang::SPIRV))
            set(_GLSLANG_TOP_LEVEL_TARGET SPIRV)

            # In case of a CMake subproject, simply link Glslang::Glslang to
            # the SPIRV target, without creating an intermediate Glslang::SPIRV
            # target.
            if(TARGET glslang)
                # Aliases of (global) targets [..] CMake 3.11 [...], as above
                set_target_properties(Glslang::Glslang PROPERTIES
                    INTERFACE_LINK_LIBRARIES SPIRV)
            # Otherwise extract everything except INTERFACE_INCLUDE_DIRECTORIES
            # (which are broken, as explained on the very top) out of
            # glslang::SPIRV into a new Glslang::SPIRV, and link
            # Glslang::Glslang to it.
            else()
                # We're setting IMPORTED_LOCATION_* below, which means the
                # library can't be INTERFACE like above.
                add_library(Glslang::SPIRV UNKNOWN IMPORTED)

                # INTERFACE_LINK_LIBRARIES link to MachineIndependent,
                # OSDependent and other loose crap in static builds, important
                # to have. Can't use
                # set_target_properties(... $<TARGET_PROPERTY:...>) because the
                # INTERFACE_LINK_LIBRARIES contain \$<LINK_ONLY: (yes,
                # escaped), causing CMake to complain that the (unevaluated)
                # generator expression isn't a target.
                get_target_property(_GLSLANG_SPIRV_INTERFACE_LINK_LIBRARIES glslang::SPIRV INTERFACE_LINK_LIBRARIES)
                # In some cases (such as Homebrew's glslang 15.2.0 package)
                # this property doesn't exist because the compatibility target
                # doesn't actually link to anything. Skip it in that case.
                if(_GLSLANG_SPIRV_INTERFACE_LINK_LIBRARIES)
                    set_target_properties(Glslang::SPIRV PROPERTIES INTERFACE_LINK_LIBRARIES "${_GLSLANG_SPIRV_INTERFACE_LINK_LIBRARIES}")
                endif()

                # Imported locations, which can be many. Assuming
                # IMPORTED_CONFIGURATIONS contains the names uppercase, as
                # IMPORTED_LOCATION_<CONFIG> is uppercase. The case is not
                # explicitly documented anywhere in the CMake manual. Can't just
                # put them all into INTERFACE_LINK_LIBRARIES because it'd mean that
                # both Debug and Release gets linked at the same time, have to
                # replicate both the IMPORTED_CONFIGURATIONS and
                # IMPORTED_LOCATION_* properties exactly
                get_target_property(_GLSLANG_SPIRV_IMPORTED_CONFIGURATIONS glslang::SPIRV IMPORTED_CONFIGURATIONS)
                set_property(TARGET Glslang::SPIRV PROPERTY IMPORTED_CONFIGURATIONS ${_GLSLANG_SPIRV_IMPORTED_CONFIGURATIONS})
                foreach(config ${_GLSLANG_SPIRV_IMPORTED_CONFIGURATIONS})
                    get_target_property(_GLSLANG_SPIRV_IMPORTED_LOCATION_${config} glslang::SPIRV IMPORTED_LOCATION_${config})
                    set_property(TARGET Glslang::SPIRV PROPERTY IMPORTED_LOCATION_${config} ${_GLSLANG_SPIRV_IMPORTED_LOCATION_${config}})
                endforeach()

                # Make the top-level target link to this recreated target
                set_target_properties(Glslang::Glslang PROPERTIES
                    INTERFACE_LINK_LIBRARIES Glslang::SPIRV)
            endif()

        # Otherwise, on version 15+, glslang is the top-level target. As the
        # SPIRV target with broken include path is gone, no target patching is
        # needed, so just directly link to it.
        else()
            set_target_properties(Glslang::Glslang PROPERTIES
                INTERFACE_LINK_LIBRARIES ${_GLSLANG_TARGET_PREFIX}glslang)
        endif()
    endif()

    # Just to make FPHSA print some meaningful location, nothing else. Luckily
    # we can just reuse what we had to find above.
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args("Glslang" DEFAULT_MSG
        _GLSLANG_INTERFACE_INCLUDE_DIRECTORIES)

    return()
endif()

# The library depends on SPIRV-Tools, except if that's disabled, which we have
# no way to check anyway.
find_package(SpirvTools REQUIRED)

# Libraries. The debug suffix is used only on Windows.
find_library(Glslang_glslang_LIBRARY_RELEASE NAMES glslang)
find_library(Glslang_glslang_LIBRARY_DEBUG NAMES glslangd)

include(SelectLibraryConfigurations)
select_library_configurations(Glslang_glslang)

# Some libraries are needed only in a static build, search for them only in
# case the main library is static (or on Windows, because there it's impossible
# to know -- in which case we'll just treat them as optional).
if(Glslang_glslang_LIBRARY MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    set(_GLSLANG_IS_STATIC ON)
endif()
if(CORRADE_TARGET_WINDOWS OR _GLSLANG_IS_STATIC)
    # ARE YOU MAD?! Why the fuck can't you put everything into a single lib FFS
    set(_GLSLANG_STATIC_LIBRARIES HLSL OSDependent OGLCompiler)
    # Update 2021-06-19: WHAT THE FUCK, why do you keep adding new fucking
    # libs?! https://github.com/KhronosGroup/glslang/pull/2301 Also, why such a
    # huge breaking change is not even hinted in the 11.0 changelog?!
    set(_GLSLANG_STATIC_LIBRARIES_11 GenericCodeGen MachineIndependent)
else()
    set(_GLSLANG_STATIC_LIBRARIES )
    set(_GLSLANG_STATIC_LIBRARIES_11 )
endif()
# For FPHSA, except the libraries that got added in glslang 11 / removed in
# glslang 15 -- we'll treat them as optional, if they are there, we link them,
# if not, we assume it's either version 10 or older or version 15+.
set(_GLSLANG_EXTRA_LIBRARIES )
foreach(_library SPIRV ${_GLSLANG_STATIC_LIBRARIES} ${_GLSLANG_STATIC_LIBRARIES_11})
    find_library(Glslang_${_library}_LIBRARY_DEBUG NAMES ${_library}d)
    find_library(Glslang_${_library}_LIBRARY_RELEASE NAMES ${_library})
    select_library_configurations(Glslang_${_library})
    if(_library IN_LIST _GLSLANG_STATIC_LIBRARIES)
        list(APPEND _GLSLANG_EXTRA_LIBRARIES Glslang_${_library}_LIBRARY)
    endif()
endforeach()

# Include dir
find_path(Glslang_INCLUDE_DIR
    # Actually, WHAT THE FUCK, I get that some people suck at naming, but this
    # is an actual naming skill black hole. Even naming it Windows.h would make
    # more sense than this. Like, what the hell.
    NAMES glslang/Public/ShaderLang.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glslang DEFAULT_MSG
    Glslang_glslang_LIBRARY ${_GLSLANG_EXTRA_LIBRARIES}
    Glslang_INCLUDE_DIR)

mark_as_advanced(FORCE Glslang_INCLUDE_DIR)

# Except for glslang these are all optional either
# -  because needed only in a static build, which is impossible to detect
# -  because some are present only in newer versions, FFS
# -  because some are no longer present in newer versions, GAH
# Additionally, in versions 14 and before, SPIRV is the top-level target, which
# then depends on glslang (and the linking order matters, yes), but in version
# 15 SPIRV is removed and glslang is the top-level target. To fix this mess,
# the glslang library is imported as a *lowercase* Glslang::glslang target so
# then the Glslang::Glslang target can link to either Glslang::SPIRV or
# Glslang::glslang.
foreach(_library glslang SPIRV ${_GLSLANG_STATIC_LIBRARIES} ${_GLSLANG_STATIC_LIBRARIES_11})
    if(Glslang_${_library}_LIBRARY AND NOT TARGET Glslang::${_library})
        add_library(Glslang::${_library} UNKNOWN IMPORTED)
        if(Glslang_${_library}_LIBRARY_RELEASE)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(Glslang::${_library} PROPERTIES
                IMPORTED_LOCATION_RELEASE ${Glslang_${_library}_LIBRARY_RELEASE})
        endif()
        if(Glslang_${_library}_LIBRARY_DEBUG)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(Glslang::${_library} PROPERTIES
                IMPORTED_LOCATION_DEBUG ${Glslang_${_library}_LIBRARY_DEBUG})
        endif()

        # SPIRV, if present, depends on SpirvTools and glslang. If not present,
        # glslang is the top-level target that depends on SpirvTools. Link
        # glslang to SpirvTools and SPIRV to glslang to cover both variants.
        if(_library STREQUAL SPIRV)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES Glslang::glslang)
        endif()
        if(_library STREQUAL glslang)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES SpirvTools::SpirvTools SpirvTools::Opt)
        endif()

        # OGLCompiler needs pthread
        if(_library STREQUAL OGLCompiler)
            find_package(Threads REQUIRED)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES Threads::Threads)
        endif()
    endif()
endforeach()

# Glslang::Glslang puts that all together
if(NOT TARGET Glslang::Glslang)
    add_library(Glslang::Glslang INTERFACE IMPORTED)
    set_target_properties(Glslang::Glslang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            # Second entry to account for broken relative includes in version
            # 7.13. Sigh. https://github.com/KhronosGroup/glslang/issues/2007
            # This also papers over the difference between paths in the repo
            # and paths actually installed (in particular
            # <glslang/SPIRV/GlslangToSpirv.h> vs <SPIRV/GlslangToSpirv.h>)
            "${Glslang_INCLUDE_DIR};${Glslang_INCLUDE_DIR}/glslang")

    # SPIRV is the top level target if exists, and depends on glslang. In
    # version 15 it's removed and glslang is the top-level target instead.
    if(TARGET Glslang::SPIRV)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::SPIRV)
    else()
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::glslang)
    endif()

    # On Windows these are all optional because a static build is impossible to
    # detect there
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::HLSL))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::HLSL)
    endif()
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::OSDependent))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OSDependent)
    endif()
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::OGLCompiler))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OGLCompiler)
    endif()
    # These two are new in 11.0, so link them only if they exist
    if((_GLSLANG_IS_STATIC OR CORRADE_TARGET_WINDOWS) AND TARGET Glslang::GenericCodeGen)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::GenericCodeGen)
    endif()
    if((_GLSLANG_IS_STATIC OR CORRADE_TARGET_WINDOWS) AND TARGET Glslang::MachineIndependent)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::MachineIndependent)
    endif()
endif()
