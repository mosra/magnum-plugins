/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "GltfSceneConverter.h"

#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/JsonWriter.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Trade/ArrayAllocator.h>

/* We'd have to endian-flip everything that goes into buffers, plus the binary
   glTF headers, etc. Too much work, hard to automatically test because the
   HW is hard to get. */
#ifdef CORRADE_TARGET_BIG_ENDIAN
#error this code will not work on Big Endian, sorry
#endif

namespace Magnum { namespace Trade {

struct GltfSceneConverter::State {
    /* Output format. Defaults for a binary output. */
    bool binary = true;
    Utility::JsonWriter::Options jsonOptions;
    UnsignedInt jsonIndentation = 0;
};

using namespace Containers::Literals;

GltfSceneConverter::GltfSceneConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractSceneConverter{manager, plugin} {}

GltfSceneConverter::~GltfSceneConverter() = default;

SceneConverterFeatures GltfSceneConverter::doFeatures() const {
    return SceneConverterFeature::ConvertMultipleToData;
}

bool GltfSceneConverter::doBeginFile(const Containers::StringView filename) {
    CORRADE_INTERNAL_ASSERT(!_state);
    _state.emplace();

    /* Decide if we're writing a text or a binary file */
    if(!configuration().value<Containers::StringView>("binary")) {
        _state->binary = Utility::String::lowercase(Utility::Path::splitExtension(filename).second()) != ".gltf"_s;
    } else _state->binary = configuration().value<bool>("binary");

    return AbstractSceneConverter::doBeginFile(filename);
}

bool GltfSceneConverter::doBeginData() {
    /* If the state is already there, it's from doBeginFile(). Otherwise create
       a new one. */
    if(!_state) {
        _state.emplace();

        /* Binary is the default for data output because we can't write
           external files. Override if the configuration is non-empty. */
        if(!configuration().value<Containers::StringView>("binary"))
            _state->binary = true;
        else
            _state->binary = configuration().value<bool>("binary");
    }

    /* Text file is pretty-printed according to options. For a binary file the
       defaults are already alright.  */
    if(!_state->binary) {
        _state->jsonOptions = Utility::JsonWriter::Option::Wrap|Utility::JsonWriter::Option::TypographicalSpace;
        _state->jsonIndentation = 2;
    }

    return true;
}

Containers::Optional<Containers::Array<char>> GltfSceneConverter::doEndData() {
    Utility::JsonWriter json{_state->jsonOptions, _state->jsonIndentation};
    json.beginObject();

    /* Asset object, always present */
    {
        json.writeKey("asset"_s);
        Containers::ScopeGuard gltfAsset = json.beginObjectScope();

        json.writeKey("version"_s).write("2.0"_s);

        if(const Containers::StringView copyright = configuration().value<Containers::StringView>("copyright"_s))
            json.writeKey("copyright"_s).write(copyright);
        if(const Containers::StringView generator = configuration().value<Containers::StringView>("generator"_s))
            json.writeKey("generator"_s).write(generator);
    }

    /* Used and required extensions */
    {
        /** @todo FFS what the stone age types here */
        const std::vector<Containers::StringView> extensionsUsed = configuration().values<Containers::StringView>("extensionUsed");
        const std::vector<Containers::StringView> extensionsRequired = configuration().values<Containers::StringView>("extensionRequired");
        if(!extensionsUsed.empty()) {
            json.writeKey("extensionsUsed"_s);
            Containers::ScopeGuard gltfExtensionsUsed = json.beginArrayScope();
            for(const Containers::StringView i: extensionsUsed) json.write(i);
        }
        if(!extensionsRequired.empty()) {
            json.writeKey("extensionsRequired"_s);
            Containers::ScopeGuard gltfExtensionsRequired = json.beginArrayScope();
            for(const Containers::StringView i: extensionsRequired) json.write(i);
        }
    }

    /* Done! */
    json.endObject();

    union CharCaster {
        UnsignedInt value;
        const char data[4];
    };

    /* Reserve the output array and write headers for a binary glTF */
    Containers::Array<char> out;
    if(_state->binary) {
        const std::size_t totalSize = 12 + /* file header */
            8 + json.size(); /* JSON chunk + header */
        Containers::arrayReserve<ArrayAllocator>(out, totalSize);

        /* glTF header */
        Containers::arrayAppend<ArrayAllocator>(out,
            Containers::ArrayView<const char>{"glTF\x02\x00\x00\x00"_s});
        /** @todo WTF the casts here */
        Containers::arrayAppend<ArrayAllocator>(out,
            Containers::arrayView(CharCaster{UnsignedInt(totalSize)}.data));

        /* JSON chunk header */
        /** @todo WTF the cast here */
        Containers::arrayAppend<ArrayAllocator>(out,
            Containers::arrayView(CharCaster{UnsignedInt(json.size())}.data));
        Containers::arrayAppend<ArrayAllocator>(out, {'J', 'S', 'O', 'N'});

    /* Otherwise reserve just for the JSON */
    } else Containers::arrayReserve<ArrayAllocator>(out, json.size());

    /* Copy the JSON data to the output. In case of a text glTF we would
       ideally just pass the memory from the JsonWriter but the class uses an
       arbitrary growable deleter internally and custom deleters are forbidden
       in plugins. */
    /** @todo make it possible to specify an external allocator in JsonWriter
        once allocators-as-arguments are a thing */
    /** @todo WTF the casts here */
    Containers::arrayAppend<ArrayAllocator>(out, Containers::ArrayView<const char>(json.toString()));

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(out));
}

void GltfSceneConverter::doAbort() {
    _state = {};
}

}}

CORRADE_PLUGIN_REGISTER(GltfSceneConverter, Magnum::Trade::GltfSceneConverter,
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.2")
