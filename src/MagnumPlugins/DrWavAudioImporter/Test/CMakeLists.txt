#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

if(CORRADE_TARGET_EMSCRIPTEN)
    set(DRWAVAUDIOIMPORTER_TEST_DIR "")
else()
    set(DRWAVAUDIOIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

corrade_add_test(DrWavAudioImporterTest DrWavImporterTest.cpp LIBRARIES MagnumDrWavAudioImporterTestLib)
target_include_directories(DrWavAudioImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
# On Win32 we need to avoid dllimporting DrWavAudioImporter symbols, because it
# would search for the symbols in some DLL even though they were linked
# statically. However it apparently doesn't matter that they were dllexported
# when building the static library. EH.
if(WIN32)
    target_compile_definitions(DrWavAudioImporterTest PRIVATE "MAGNUM_DRWAVAUDIOIMPORTER_BUILD_STATIC")
endif()

if(CORRADE_TARGET_EMSCRIPTEN)
    emscripten_embed_file(DrWavAudioImporterTest unsupportedChannelCount.wav "/unsupportedChannelCount.wav")
    emscripten_embed_file(DrWavAudioImporterTest unsupportedFormat.wav "/unsupportedFormat.wav")
    emscripten_embed_file(DrWavAudioImporterTest wrongSignature.wav "/wrongSignature.wav")

    emscripten_embed_file(DrWavAudioImporterTest invalidPadding.wav "/invalidPadding.wav")
    emscripten_embed_file(DrWavAudioImporterTest invalidLength.wav "/invalidLength.wav")
    emscripten_embed_file(DrWavAudioImporterTest invalidDataChunk.wav "/invalidDataChunk.wav")
    emscripten_embed_file(DrWavAudioImporterTest invalidFactChunk.wav "/invalidFactChunk.wav")

    emscripten_embed_file(DrWavAudioImporterTest mono8.wav "/mono8.wav")
    emscripten_embed_file(DrWavAudioImporterTest mono8.wav "/mono8junk.wav")

    emscripten_embed_file(DrWavAudioImporterTest mono8ALaw.wav "/mono8ALaw.wav")
    emscripten_embed_file(DrWavAudioImporterTest mono8MuLaw.wav "/mono8MuLaw.wav")
    emscripten_embed_file(DrWavAudioImporterTest mono16.wav "/mono16.wav")

    emscripten_embed_file(DrWavAudioImporterTest stereo8.wav "/stereo8.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo8ALaw.wav "/stereo8ALaw.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo8MuLaw.wav "/stereo8MuLaw.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo12.wav "/stereo12.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo16.wav "/stereo16.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo24.wav "/stereo24.wav")

    emscripten_embed_file(DrWavAudioImporterTest mono32f.wav "/mono32f.wav")
    emscripten_embed_file(DrWavAudioImporterTest stereo32f.wav "/stereo32f.wav")

    emscripten_embed_file(DrWavAudioImporterTest stereo64f.wav "/stereo64f.wav")

    emscripten_embed_file(DrWavAudioImporterTest surround616.wav "/surround616.wav")
endif()
