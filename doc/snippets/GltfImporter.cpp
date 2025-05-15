/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>

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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Json.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#define DOXYGEN_ELLIPSIS(...) __VA_ARGS__

using namespace Magnum;

/* GCC 11+ in Release warns that "this pointer is null". Yes. It is. Fuck off,
   those are documentation code snippets. */
#if defined(CORRADE_TARGET_GCC) && !defined(CORRADE_TARGET_CLANG) && __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Wnonnull"
#endif

int main() {

{
/* [importerState] */
Containers::Pointer<Trade::AbstractImporter> importer = DOXYGEN_ELLIPSIS({});
Containers::Optional<Trade::MeshData> mesh = importer->mesh(DOXYGEN_ELLIPSIS(0));

/* Get a mutable reference to the Json instance so we can use it to parse */
Utility::Json& gltf = *static_cast<Utility::Json*>(
    const_cast<void*>(importer->importerState())
);

/* Get the outline indices accessor, if present. Can't assume anything is
   parsed, so call parseObject() and parseUnsignedInt() before accessing every
   value. */
Utility::JsonToken gltfMesh{gltf,
    *static_cast<const Utility::JsonTokenData*>(mesh->importerState())};
Containers::Optional<UnsignedInt> indices;
if(Utility::JsonIterator gltfExtensions =
    gltf.parseObject(gltfMesh)->find("extensions"))
{
    if(Utility::JsonIterator gltfCesiumPrimitiveOutline =
        gltf.parseObject(*gltfExtensions)->find("CESIUM_primitive_outline"))
    {
        indices = gltf.parseUnsignedInt((
            *gltf.parseObject(*gltfCesiumPrimitiveOutline))["indices"]);
    }
}
/* [importerState] */
}

}
