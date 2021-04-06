#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
macro(add_plugin plugin_name debug_install_dirs release_install_dirs metadata_file)
    string(TOUPPER ${plugin_name} PLUGIN_NAME)
    if(NOT MAGNUM_${PLUGIN_NAME}_BUILD_STATIC)
        corrade_add_plugin(${plugin_name} "${debug_install_dirs}" "${release_install_dirs}" ${metadata_file} ${ARGN})
        set_target_properties(${plugin_name} ${plugin_name}-metadata PROPERTIES FOLDER "MagnumPlugins/${plugin_name}")
    else()
        corrade_add_static_plugin(${plugin_name} "${release_install_dirs}" ${metadata_file} ${ARGN})
        set_target_properties(${plugin_name} ${plugin_name}-dependencies PROPERTIES FOLDER "MagnumPlugins/${plugin_name}")
    endif()
endmacro()

if(WITH_ASSIMPIMPORTER)
    add_subdirectory(AssimpImporter)
endif()

if(WITH_BASISIMAGECONVERTER)
    add_subdirectory(BasisImageConverter)
endif()

if(WITH_BASISIMPORTER)
    add_subdirectory(BasisImporter)
endif()

if(WITH_DDSIMPORTER)
    add_subdirectory(DdsImporter)
endif()

if(WITH_DEVILIMAGEIMPORTER)
    add_subdirectory(DevIlImageImporter)
endif()

if(WITH_DRFLACAUDIOIMPORTER)
    add_subdirectory(DrFlacAudioImporter)
endif()

if(WITH_DRMP3AUDIOIMPORTER)
    add_subdirectory(DrMp3AudioImporter)
endif()

if(WITH_DRWAVAUDIOIMPORTER)
    add_subdirectory(DrWavAudioImporter)
endif()

if(WITH_FAAD2AUDIOIMPORTER)
    add_subdirectory(Faad2AudioImporter)
endif()

if(WITH_FREETYPEFONT)
    add_subdirectory(FreeTypeFont)
endif()

if(WITH_GLSLANGSHADERCONVERTER)
    add_subdirectory(GlslangShaderConverter)
endif()

if(WITH_HARFBUZZFONT)
    add_subdirectory(HarfBuzzFont)
endif()

if(WITH_ICOIMPORTER)
    add_subdirectory(IcoImporter)
endif()

if(WITH_JPEGIMAGECONVERTER)
    add_subdirectory(JpegImageConverter)
endif()

if(WITH_JPEGIMPORTER)
    add_subdirectory(JpegImporter)
endif()

if(WITH_MESHOPTIMIZERSCENECONVERTER)
    add_subdirectory(MeshOptimizerSceneConverter)
endif()

if(WITH_MINIEXRIMAGECONVERTER)
    add_subdirectory(MiniExrImageConverter)
endif()

if(WITH_OPENGEXIMPORTER)
    add_subdirectory(OpenGexImporter)
endif()

if(WITH_OPENEXRIMAGECONVERTER)
    add_subdirectory(OpenExrImageConverter)
endif()

if(WITH_OPENEXRIMPORTER)
    add_subdirectory(OpenExrImporter)
endif()

if(WITH_PNGIMAGECONVERTER)
    add_subdirectory(PngImageConverter)
endif()

if(WITH_PNGIMPORTER)
    add_subdirectory(PngImporter)
endif()

if(WITH_PRIMITIVEIMPORTER)
    add_subdirectory(PrimitiveImporter)
endif()

if(WITH_SPIRVTOOLSSHADERCONVERTER)
    add_subdirectory(SpirvToolsShaderConverter)
endif()

if(WITH_STANFORDIMPORTER)
    add_subdirectory(StanfordImporter)
endif()

if(WITH_STANFORDSCENECONVERTER)
    add_subdirectory(StanfordSceneConverter)
endif()

if(WITH_STBIMAGECONVERTER)
    add_subdirectory(StbImageConverter)
endif()

if(WITH_STBIMAGEIMPORTER)
    add_subdirectory(StbImageImporter)
endif()

if(WITH_STBTRUETYPEFONT)
    add_subdirectory(StbTrueTypeFont)
endif()

if(WITH_STBVORBISAUDIOIMPORTER)
    add_subdirectory(StbVorbisAudioImporter)
endif()

if(WITH_STLIMPORTER)
    add_subdirectory(StlImporter)
endif()

if(WITH_TINYGLTFIMPORTER)
    add_subdirectory(TinyGltfImporter)
endif()
