#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>
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

find_package(DevIL REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(DevIlImageImporter_SRCS
    DevIlImageImporter.cpp)

set(DevIlImageImporter_HEADERS
    DevIlImageImporter.h)

# Objects shared between plugin and test library
add_library(DevIlImageImporterObjects OBJECT
    ${DevIlImageImporter_SRCS}
    ${DevIlImageImporter_HEADERS})
target_include_directories(DevIlImageImporterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(DevIlImageImporterObjects PRIVATE "DevIlImageImporterObjects_EXPORTS")
# IL_INCLUDE_DIR in the upstream FindDevIL.cmake is "The path to the directory
# that contains il.h", but ilu.h includes IL/il.h, so IL_INCLUDE_DIR is
# basically useless and we need to go one level up. However, just saying
# ${IL_INCLUDE_DIR}/.. here wouldn't work, because this directory is included
# as system and saying `-isystem /usr/include` hits
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70129 on GCC 6+. Because of
# that, I had to fork FindDevIL.cmake (see modules/ directory) to provide
# IL_INCLUDE_DIR without the IL/ subdirectory.
target_include_directories(DevIlImageImporterObjects SYSTEM PRIVATE ${IL_INCLUDE_DIR})
set_target_properties(DevIlImageImporterObjects PROPERTIES FOLDER "MagnumPlugins/DevIlImageImporter")
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(DevIlImageImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# DevIlImageImporter plugin
add_plugin(DevIlImageImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    DevIlImageImporter.conf
    $<TARGET_OBJECTS:DevIlImageImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(DevIlImageImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(DevIlImageImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(DevIlImageImporter
    Magnum::Magnum
    ${IL_LIBRARIES} ${ILU_LIBRARIES})

install(FILES ${DevIlImageImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/DevIlImageImporter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/DevIlImageImporter)

if(BUILD_TESTS)
    add_library(MagnumDevIlImageImporterTestLib STATIC
        $<TARGET_OBJECTS:DevIlImageImporterObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumDevIlImageImporterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumDevIlImageImporterTestLib PROPERTIES FOLDER "MagnumPlugins/DevIlImageImporter")
    target_link_libraries(MagnumDevIlImageImporterTestLib Magnum::Magnum ${IL_LIBRARIES} ${ILU_LIBRARIES})
    add_subdirectory(Test)
endif()

# MagnumPlugins DevIlImageImporter target alias for superprojects
add_library(MagnumPlugins::DevIlImageImporter ALIAS DevIlImageImporter)
