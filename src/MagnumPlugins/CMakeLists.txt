#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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

# Wrapper for creating given plugin type
macro(add_plugin plugin_name plugin_folder debug_install_dirs release_install_dirs metadata_file)
    string(TOUPPER ${plugin_name} PLUGIN_NAME)
    if(NOT MAGNUM_${PLUGIN_NAME}_BUILD_STATIC)
        corrade_add_plugin(${plugin_name} "${debug_install_dirs}" "${release_install_dirs}" ${metadata_file} ${ARGN})
        set_target_properties(${plugin_name} ${plugin_name}-metadata PROPERTIES FOLDER "MagnumPlugins/${plugin_name}")
    else()
        corrade_add_static_plugin(${plugin_name} "${release_install_dirs}" ${metadata_file} ${ARGN})
        set_target_properties(${plugin_name} ${plugin_name}-dependencies PROPERTIES FOLDER "MagnumPlugins/${plugin_name}")
    endif()
    # For each type of output directory and each configuration, append the plugin subfolder only if the global CMAKE_ variable was set
    foreach(output RUNTIME LIBRARY ARCHIVE)
        if(CMAKE_${output}_OUTPUT_DIRECTORY)
            set_target_properties(${plugin_name} PROPERTIES
                ${output}_OUTPUT_DIRECTORY ${CMAKE_${output}_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/${plugin_folder})
        endif()
        # This doesn't handle custom build types as there's no way to know their names
        foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            if(CMAKE_${output}_OUTPUT_DIRECTORY_${config})
                set_target_properties(${plugin_name} PROPERTIES
                    ${output}_OUTPUT_DIRECTORY_${config} ${CMAKE_${output}_OUTPUT_DIRECTORY_${config}}/magnum$<$<CONFIG:Debug>:-d>/${plugin_folder})
            endif()
        endforeach()
    endforeach()
endmacro()

if(MAGNUM_WITH_ASSIMPIMPORTER)
    add_subdirectory(AssimpImporter)
endif()

if(MAGNUM_WITH_ASTCIMPORTER)
    add_subdirectory(AstcImporter)
endif()

if(MAGNUM_WITH_BASISIMAGECONVERTER)
    add_subdirectory(BasisImageConverter)
endif()

if(MAGNUM_WITH_BASISIMPORTER)
    add_subdirectory(BasisImporter)
endif()

if(MAGNUM_BUILD_DEPRECATED AND MAGNUM_WITH_CGLTFIMPORTER)
    add_subdirectory(CgltfImporter)
endif()

if(MAGNUM_WITH_DDSIMPORTER)
    add_subdirectory(DdsImporter)
endif()

if(MAGNUM_WITH_DEVILIMAGEIMPORTER)
    add_subdirectory(DevIlImageImporter)
endif()

if(MAGNUM_WITH_DRFLACAUDIOIMPORTER)
    add_subdirectory(DrFlacAudioImporter)
endif()

if(MAGNUM_WITH_DRMP3AUDIOIMPORTER)
    add_subdirectory(DrMp3AudioImporter)
endif()

if(MAGNUM_WITH_DRWAVAUDIOIMPORTER)
    add_subdirectory(DrWavAudioImporter)
endif()

if(MAGNUM_WITH_FAAD2AUDIOIMPORTER)
    add_subdirectory(Faad2AudioImporter)
endif()

if(MAGNUM_WITH_FREETYPEFONT)
    add_subdirectory(FreeTypeFont)
endif()

if(MAGNUM_WITH_GLSLANGSHADERCONVERTER)
    add_subdirectory(GlslangShaderConverter)
endif()

if(MAGNUM_WITH_GLTFIMPORTER)
    add_subdirectory(GltfImporter)
endif()

if(MAGNUM_WITH_GLTFSCENECONVERTER)
    add_subdirectory(GltfSceneConverter)
endif()

if(MAGNUM_WITH_HARFBUZZFONT)
    add_subdirectory(HarfBuzzFont)
endif()

if(MAGNUM_WITH_ICOIMPORTER)
    add_subdirectory(IcoImporter)
endif()

if(MAGNUM_WITH_JPEGIMAGECONVERTER)
    add_subdirectory(JpegImageConverter)
endif()

if(MAGNUM_WITH_JPEGIMPORTER)
    add_subdirectory(JpegImporter)
endif()

if(MAGNUM_WITH_KTXIMAGECONVERTER)
    add_subdirectory(KtxImageConverter)
endif()

if(MAGNUM_WITH_KTXIMPORTER)
    add_subdirectory(KtxImporter)
endif()

if(MAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER)
    add_subdirectory(MeshOptimizerSceneConverter)
endif()

if(MAGNUM_WITH_MINIEXRIMAGECONVERTER)
    add_subdirectory(MiniExrImageConverter)
endif()

if(MAGNUM_WITH_OPENGEXIMPORTER)
    add_subdirectory(OpenGexImporter)
endif()

if(MAGNUM_WITH_OPENEXRIMAGECONVERTER)
    add_subdirectory(OpenExrImageConverter)
endif()

if(MAGNUM_WITH_OPENEXRIMPORTER)
    add_subdirectory(OpenExrImporter)
endif()

if(MAGNUM_WITH_PNGIMAGECONVERTER)
    add_subdirectory(PngImageConverter)
endif()

if(MAGNUM_WITH_PNGIMPORTER)
    add_subdirectory(PngImporter)
endif()

if(MAGNUM_WITH_PRIMITIVEIMPORTER)
    add_subdirectory(PrimitiveImporter)
endif()

if(MAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER)
    add_subdirectory(SpirvToolsShaderConverter)
endif()

if(MAGNUM_WITH_STANFORDIMPORTER)
    add_subdirectory(StanfordImporter)
endif()

if(MAGNUM_WITH_STANFORDSCENECONVERTER)
    add_subdirectory(StanfordSceneConverter)
endif()

if(MAGNUM_WITH_STBDXTIMAGECONVERTER)
    add_subdirectory(StbDxtImageConverter)
endif()

if(MAGNUM_WITH_STBIMAGECONVERTER)
    add_subdirectory(StbImageConverter)
endif()

if(MAGNUM_WITH_STBIMAGEIMPORTER)
    add_subdirectory(StbImageImporter)
endif()

if(MAGNUM_WITH_STBRESIZEIMAGECONVERTER)
    add_subdirectory(StbResizeImageConverter)
endif()

if(MAGNUM_WITH_STBTRUETYPEFONT)
    add_subdirectory(StbTrueTypeFont)
endif()

if(MAGNUM_WITH_STBVORBISAUDIOIMPORTER)
    add_subdirectory(StbVorbisAudioImporter)
endif()

if(MAGNUM_WITH_STLIMPORTER)
    add_subdirectory(StlImporter)
endif()

if(MAGNUM_BUILD_DEPRECATED AND MAGNUM_WITH_TINYGLTFIMPORTER)
    add_subdirectory(TinyGltfImporter)
endif()

if(MAGNUM_WITH_WEBPIMPORTER)
    add_subdirectory(WebPImporter)
endif()
