#.rst:
# Find Corrade
# ------------
#
# Finds the Corrade library. Basic usage::
#
#  find_package(Corrade [REQUIRED])
#
# This module tries to find the base Corrade library and then defines the
# following:
#
#  Corrade_FOUND                - Whether the base library was found
#  CORRADE_LIB_SUFFIX_MODULE    - Path to CorradeLibSuffix.cmake module
#
# This command will try to find only the base library, not the optional
# components, which are:
#
#  Containers                   - Containers library
#  PluginManager                - PluginManager library
#  TestSuite                    - TestSuite library
#  Utility                      - Utility library
#  rc                           - corrade-rc executable
#
# Example usage with specifying additional components is::
#
#  find_package(Corrade [REQUIRED|COMPONENTS]
#               Utility TestSuite)
#
# For each component is then defined:
#
#  CORRADE_*_FOUND              - Whether the component was found
#  Corrade::*                   - Component imported target
#
# The package is found if either debug or release version of each library is
# found. If both debug and release libraries are found, proper version is
# chosen based on actual build configuration of the project (i.e. Debug build
# is linked to debug libraries, Release build to release libraries).
#
# On multi-configuration build systems (such as Visual Studio or XCode) the
# preprocessor variable ``CORRADE_IS_DEBUG_BUILD`` is defined if given build
# configuration is Debug (not Corrade itself, but build configuration of the
# project using it). Useful e.g. for selecting proper plugin directory. On
# single-configuration build systems (such as Makefiles) this information is
# not needed and thus the variable is not defined in any case.
#
# If you link to any Corrade library, the compiler will be configured to use
# the C++11 standard (if not already). Additionally you can use
# ``CORRADE_CXX_FLAGS`` to enable additional pedantic set of warnings and
# enable hidden visibility by default.
#
# Features of found Corrade library are exposed in these variables::
#
#  CORRADE_GCC47_COMPATIBILITY  - Defined if compiled with compatibility mode
#   for GCC 4.7
#  CORRADE_MSVC2015_COMPATIBILITY  - Defined if compiled with compatibility
#   mode for MSVC 2015
#  CORRADE_BUILD_DEPRECATED     - Defined if compiled with deprecated APIs
#   included
#  CORRADE_BUILD_STATIC         - Defined if compiled as static libraries.
#   Default are shared libraries.
#  CORRADE_TARGET_UNIX          - Defined if compiled for some Unix flavor
#   (Linux, BSD, OS X)
#  CORRADE_TARGET_APPLE         - Defined if compiled for Apple platforms
#  CORRADE_TARGET_IOS           - Defined if compiled for iOS
#  CORRADE_TARGET_WINDOWS       - Defined if compiled for Windows
#  CORRADE_TARGET_WINDOWS_RT    - Defined if compiled for Windows RT
#  CORRADE_TARGET_NACL          - Defined if compiled for Google Chrome Native
#   Client
#  CORRADE_TARGET_NACL_NEWLIB   - Defined if compiled for Google Chrome Native
#   Client with `newlib` toolchain
#  CORRADE_TARGET_NACL_GLIBC    - Defined if compiled for Google Chrome Native
#   Client with `glibc` toolchain
#  CORRADE_TARGET_EMSCRIPTEN    - Defined if compiled for Emscripten
#  CORRADE_TARGET_ANDROID       - Defined if compiled for Android
#
# Additionally these variables are defined for internal usage::
#
#  CORRADE_INCLUDE_DIR          - Root include dir
#  CORRADE_*_LIBRARY_DEBUG      - Debug version of given library, if found
#  CORRADE_*_LIBRARY_RELEASE    - Release version of given library, if found
#  CORRADE_USE_MODULE           - Path to UseCorrade.cmake module (included
#   automatically)
#
# Workflows without :prop_tgt:`IMPORTED` targets are deprecated and the
# following variables are included just for backwards compatibility and only if
# ``CORRADE_BUILD_DEPRECATED`` is enabled:
#
#  CORRADE_INCLUDE_DIRS         - Include directories
#  CORRADE_*_LIBRARIES          - Expands to Corrade::* target
#
# Corrade provides these macros and functions:
#
# Add unit test using Corrade's TestSuite
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# ::
#
#  corrade_add_test(test_name
#                   sources...
#                   [LIBRARIES libraries...])
#
# Test name is also executable name. You can also specify libraries to link
# with instead of using :command:`target_link_libraries()`.
# ``Corrade::TestSuite`` library is linked automatically to each test. Note
# that the :command:`enable_testing()` function must be called explicitly.
#
# Compile data resources into application binary
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# ::
#
#  corrade_add_resource(name resources.conf)
#
# Depends on ``Corrade::rc``, which is part of Corrade utilities. This command
# generates resource data using given configuration file in current build
# directory. Argument name is name under which the resources can be explicitly
# loaded. Variable ``name`` contains compiled resource filename, which is then
# used for compiling library / executable. Example usage::
#
#  corrade_add_resource(app_resources resources.conf)
#  add_executable(app source1 source2 ... ${app_resources})
#
# Add dynamic plugin
# ^^^^^^^^^^^^^^^^^^
#
# ::
#
#  corrade_add_plugin(plugin_name debug_install_dir release_install_dir
#                     metadata_file sources...)
#
# The macro adds preprocessor directive ``CORRADE_DYNAMIC_PLUGIN``. Additional
# libraries can be linked in via :command:`target_link_libraries(plugin_name ...) <target_link_libraries>`.
# If ``debug_install_dir`` is set to :variable:`CMAKE_CURRENT_BINARY_DIR` (e.g.
# for testing purposes), the files are copied directly, without the need to
# perform install step. Note that the files are actually put into
# configuration-based subdirectory, i.e. ``${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}``.
# See documentation of :variable:`CMAKE_CFG_INTDIR` variable for more
# information.
#
# Add static plugin
# ^^^^^^^^^^^^^^^^^
#
# ::
#
#  corrade_add_static_plugin(plugin_name install_dir metadata_file
#                            sources...)
#
# The macro adds preprocessor directive ``CORRADE_STATIC_PLUGIN``. Additional
# libraries can be linked in via :command:`target_link_libraries(plugin_name ...) <target_link_libraries>`.
# If ``install_dir`` is set to :variable:`CMAKE_CURRENT_BINARY_DIR` (e.g. for
# testing purposes), no installation rules are added.
#
# Note that plugins built in debug configuration (e.g. with :variable:`CMAKE_BUILD_TYPE`
# set to ``Debug``) have ``"-d"`` suffix to make it possible to have both debug
# and release plugins installed alongside each other.
#
# Find corresponding DLLs for library files
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# ::
#
#  corrade_find_dlls_for_libs(<VAR> libs...)
#
# Available only on Windows, for all ``*.lib`` files tries to find
# corresponding DLL file. Useful for bundling dependencies for e.g. WinRT
# packages.
#

#
#   This file is part of Corrade.
#
#   Copyright © 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

# Root include dir
find_path(CORRADE_INCLUDE_DIR
    NAMES Corrade/Corrade.h)

# Configuration file
find_file(_CORRADE_CONFIGURE_FILE configure.h
    HINTS ${CORRADE_INCLUDE_DIR}/Corrade/)
mark_as_advanced(_CORRADE_CONFIGURE_FILE)

# We need to open configure.h file from CORRADE_INCLUDE_DIR before we check for
# the components. Bail out with proper error message if it wasn't found. The
# complete check with all components is further below.
if(NOT CORRADE_INCLUDE_DIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Corrade
        REQUIRED_VARS CORRADE_INCLUDE_DIR _CORRADE_CONFIGURE_FILE)
endif()

# Read flags from configuration
file(READ ${_CORRADE_CONFIGURE_FILE} _corradeConfigure)
set(_corradeFlags
    GCC47_COMPATIBILITY
    MSVC2015_COMPATIBILITY
    BUILD_DEPRECATED
    BUILD_STATIC
    TARGET_UNIX
    TARGET_APPLE
    TARGET_IOS
    TARGET_WINDOWS
    TARGET_WINDOWS_RT
    TARGET_NACL
    TARGET_NACL_NEWLIB
    TARGET_NACL_GLIBC
    TARGET_EMSCRIPTEN
    TARGET_ANDROID)
foreach(_corradeFlag ${_corradeFlags})
    string(FIND "${_corradeConfigure}" "#define CORRADE_${_corradeFlag}" _corrade_${_corradeFlag})
    if(NOT _corrade_${_corradeFlag} EQUAL -1)
        set(CORRADE_${_corradeFlag} 1)
    endif()
endforeach()

# CMake module dir
find_path(_CORRADE_MODULE_DIR
    NAMES UseCorrade.cmake CorradeLibSuffix.cmake
    PATH_SUFFIXES share/cmake/Corrade)
mark_as_advanced(_CORRADE_MODULE_DIR)

set(CORRADE_USE_MODULE ${_CORRADE_MODULE_DIR}/UseCorrade.cmake)
set(CORRADE_LIB_SUFFIX_MODULE ${_CORRADE_MODULE_DIR}/CorradeLibSuffix.cmake)

# Ensure that all inter-component dependencies are specified as well
foreach(component ${Corrade_FIND_COMPONENTS})
    string(TOUPPER ${component} _COMPONENT)

    if(component STREQUAL Containers)
        set(_CORRADE_${_COMPONENT}_DEPENDENCIES Utility)
    elseif(component STREQUAL Interconnect)
        set(_CORRADE_${_COMPONENT}_DEPENDENCIES Utility)
    elseif(component STREQUAL PluginManager)
        set(_CORRADE_${_COMPONENT}_DEPENDENCIES Containers Utility)
    elseif(component STREQUAL TestSuite)
        set(_CORRADE_${_COMPONENT}_DEPENDENCIES Utility)
    elseif(component STREQUAL Utility)
        set(_CORRADE_${_COMPONENT}_DEPENDENCIES Containers rc)
    endif()

    list(APPEND _CORRADE_ADDITIONAL_COMPONENTS ${_CORRADE_${_COMPONENT}_DEPENDENCIES})
endforeach()

# Join the lists, remove duplicate components
if(_CORRADE_ADDITIONAL_COMPONENTS)
    list(INSERT Corrade_FIND_COMPONENTS 0 ${_CORRADE_ADDITIONAL_COMPONENTS})
endif()
if(Corrade_FIND_COMPONENTS)
    list(REMOVE_DUPLICATES Corrade_FIND_COMPONENTS)
endif()

# Component distinction
set(_CORRADE_LIBRARY_COMPONENTS "^(Containers|Interconnect|PluginManager|TestSuite|Utility)$")
set(_CORRADE_HEADER_ONLY_COMPONENTS "^(Containers)$")
set(_CORRADE_EXECUTABLE_COMPONENTS "^(rc)$")

# Find all components
foreach(_component ${Corrade_FIND_COMPONENTS})
    string(TOUPPER ${_component} _COMPONENT)

    # Create imported target in case the library is found. If the project is
    # added as subproject to CMake, the target already exists and all the
    # required setup is already done from the build tree.
    if(TARGET Corrade::${_component})
        set(Corrade_${_component}_FOUND ON)
    else()
        # Library components
        if(_component MATCHES ${_CORRADE_LIBRARY_COMPONENTS} AND NOT _component MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS})
            add_library(Corrade::${_component} UNKNOWN IMPORTED)

            # Try to find both debug and release version
            find_library(CORRADE_${_COMPONENT}_LIBRARY_DEBUG Corrade${_component}-d)
            find_library(CORRADE_${_COMPONENT}_LIBRARY_RELEASE Corrade${_component})
            mark_as_advanced(CORRADE_${_COMPONENT}_LIBRARY_DEBUG
                CORRADE_${_COMPONENT}_LIBRARY_RELEASE)

            if(CORRADE_${_COMPONENT}_LIBRARY_RELEASE)
                set_property(TARGET Corrade::${_component} APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
                set_target_properties(Corrade::${_component} PROPERTIES
                    IMPORTED_LOCATION_RELEASE ${CORRADE_${_COMPONENT}_LIBRARY_RELEASE})
            endif()

            if(CORRADE_${_COMPONENT}_LIBRARY_DEBUG)
                set_property(TARGET Corrade::${_component} APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
                set_target_properties(Corrade::${_component} PROPERTIES
                    IMPORTED_LOCATION_DEBUG ${CORRADE_${_COMPONENT}_LIBRARY_DEBUG})
            endif()
        endif()

        # Header-only library components (CMake >= 3.0)
        if(_component MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS} AND NOT CMAKE_VERSION VERSION_LESS 3.0.0)
            add_library(Corrade::${_component} INTERFACE IMPORTED)
        endif()

        # Executable components
        if(_component MATCHES ${_CORRADE_EXECUTABLE_COMPONENTS})
            add_executable(Corrade::${_component} IMPORTED)

            find_program(CORRADE_${_COMPONENT}_EXECUTABLE corrade-${_component})
            mark_as_advanced(CORRADE_${_COMPONENT}_EXECUTABLE)

            if(CORRADE_${_COMPONENT}_EXECUTABLE)
                set_property(TARGET Corrade::${_component} PROPERTY
                    IMPORTED_LOCATION ${CORRADE_${_COMPONENT}_EXECUTABLE})
            endif()
        endif()

        # No special setup for Containers library
        # No special setup for Interconnect library

        # PluginManager library
        if(_component STREQUAL PluginManager)
            # At least static build needs this
            if(CORRADE_TARGET_UNIX OR CORRADE_TARGET_NACL_GLIBC)
                set_property(TARGET Corrade::${_component}
                    APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${CMAKE_DL_LIBS})
            endif()

        # No special setup for TestSuite library

        # Utility library (contains all setup that is used by others)
        elseif(_component STREQUAL Utility)
            # Top-level include directory
            set_property(TARGET Corrade::Utility PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                ${CORRADE_INCLUDE_DIR})

            # If the configure file is somewhere else than in root include dir (e.g. when
            # using CMake subproject), we need to include that dir too
            if(NOT ${CORRADE_INCLUDE_DIR}/Corrade/configure.h STREQUAL ${_CORRADE_CONFIGURE_FILE})
                # Go two levels up
                get_filename_component(_CORRADE_CONFIGURE_FILE_INCLUDE_DIR ${_CORRADE_CONFIGURE_FILE} DIRECTORY)
                get_filename_component(_CORRADE_CONFIGURE_FILE_INCLUDE_DIR ${_CORRADE_CONFIGURE_FILE_INCLUDE_DIR} DIRECTORY)
                set_property(TARGET Corrade::Utility APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES
                    ${_CORRADE_CONFIGURE_FILE_INCLUDE_DIR})
            endif()

            # Enable C++11 on GCC/Clang. Allow the users to override this, so in case the
            # user specified CXX_STANDARD property or put "-std=" in CMAKE_CXX_FLAGS
            # nothing would be added. It doesn't cover adding flags using
            # target_compile_options(), but that didn't work before anyway.
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang" OR CORRADE_TARGET_EMSCRIPTEN)
                if(NOT CMAKE_CXX_FLAGS MATCHES "-std=")
                    # TODO: CMake 3.1 has CMAKE_CXX_STANDARD and CMAKE_CXX_STANDARD_REQUIRED
                    set_property(TARGET Corrade::Utility PROPERTY INTERFACE_COMPILE_OPTIONS
                        "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:CXX_STANDARD>>>:-std=c++11>")
                endif()
            endif()

            # Use C++11-enabled libcxx on OSX. Again, it only covers overriding the
            # "-stdlib=" in CMAKE_CXX_FLAGS and nowhere else.
            if(CORRADE_TARGET_APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang" AND NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=")
                set_property(TARGET Corrade::Utility APPEND PROPERTY INTERFACE_COMPILE_OPTIONS "-stdlib=libc++")
                set_property(TARGET Corrade::Utility PROPERTY INTERFACE_LINK_LIBRARIES c++)
            endif()

            # AndroidLogStreamBuffer class needs to be linked to log library
            if(CORRADE_TARGET_ANDROID)
                set_property(TARGET Corrade::${_component}
                    APPEND PROPERTY INTERFACE_LINK_LIBRARIES log)
            endif()
        endif()

        # Find library includes
        if(_component MATCHES ${_CORRADE_LIBRARY_COMPONENTS})
            find_path(_CORRADE_${_COMPONENT}_INCLUDE_DIR
                NAMES ${_component}.h
                HINTS ${CORRADE_INCLUDE_DIR}/Corrade/${_component})
            mark_as_advanced(_CORRADE_${_COMPONENT}_INCLUDE_DIR)
        endif()

        # Add inter-library dependencies (except for the header-only libraries
        # on 2.8.12)
        if(_component MATCHES ${_CORRADE_LIBRARY_COMPONENTS} AND (NOT CMAKE_VERSION VERSION_LESS 3.0.0 OR NOT _component MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS}))
            foreach(_dependency ${_CORRADE_${_COMPONENT}_DEPENDENCIES})
                if(_dependency MATCHES ${_CORRADE_LIBRARY_COMPONENTS} AND (NOT CMAKE_VERSION VERSION_LESS 3.0.0 OR NOT _dependency MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS}))
                    set_property(TARGET Corrade::${_component} APPEND PROPERTY
                        INTERFACE_LINK_LIBRARIES Corrade::${_dependency})
                endif()
            endforeach()
        endif()

        # Decide if the component was found
        if((_component MATCHES ${_CORRADE_LIBRARY_COMPONENTS} AND _CORRADE_${_COMPONENT}_INCLUDE_DIR AND (_component MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS} OR CORRADE_${_COMPONENT}_LIBRARY_RELEASE OR CORRADE_${_COMPONENT}_LIBRARY_DEBUG)) OR (_component MATCHES ${_CORRADE_EXECUTABLE_COMPONENTS} AND CORRADE_${_COMPONENT}_EXECUTABLE))
            set(Corrade_${_component}_FOUND True)
        else()
            set(Corrade_${_component}_FOUND False)
        endif()
    endif()

    # Deprecated variables
    if(CORRADE_BUILD_DEPRECATED AND _component MATCHES ${_CORRADE_LIBRARY_COMPONENTS} AND NOT _component MATCHES ${_CORRADE_HEADER_ONLY_COMPONENTS})
        set(CORRADE_${_COMPONENT}_LIBRARIES Corrade::${_component})
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Corrade
    REQUIRED_VARS CORRADE_INCLUDE_DIR _CORRADE_MODULE_DIR _CORRADE_CONFIGURE_FILE
    HANDLE_COMPONENTS)

# Deprecated variables
if(CORRADE_BUILD_DEPRECATED)
    set(CORRADE_INCLUDE_DIRS ${CORRADE_INCLUDE_DIR} ${_CORRADE_CONFIGURE_FILE_INCLUDE_DIR})
endif()

# Finalize the finding process
include(${CORRADE_USE_MODULE})
