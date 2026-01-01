/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "PlutoSvgImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Trade/ImageData.h>

#include <plutosvg.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

struct PlutoSvgImporter::State {
    ~State() {
        plutosvg_document_destroy(document);
    }

    /* Disable copies and moves to avoid accidental double deletion */
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    Containers::Array<char> data;
    plutosvg_document* document;
};

PlutoSvgImporter::PlutoSvgImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin) : AbstractImporter{manager, plugin} {}

PlutoSvgImporter::~PlutoSvgImporter() = default;

ImporterFeatures PlutoSvgImporter::doFeatures() const {
    return ImporterFeature::OpenData;
}

bool PlutoSvgImporter::doIsOpened() const {
    /* If the state exists, the document should too */
    CORRADE_INTERNAL_ASSERT(!_state || _state->document);
    return !!_state;
}

void PlutoSvgImporter::doClose() { _state = nullptr; }

void PlutoSvgImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* PlutoSVG doesn't make a copy of the memory (which is admirable!) so take
       over the existing array or copy the data if we can't */
    Containers::Array<char> copy;
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned))
        copy = Utility::move(data);
    else
        copy = Containers::Array<char>{InPlaceInit, data};

    /* Uh, but, where do I get to access some failure state? */
    plutosvg_document* const document = plutosvg_document_load_from_data(copy.data(), copy.size(), -1.0f, -1.0f, nullptr, nullptr);
    if(!document) {
        Error{} << "Trade::PlutoSvgImporter::openData(): parsing failed";
        return;
    }

    /* Everything is okay, save the state */
    _state.emplace(Utility::move(copy), Utility::move(document));
}

UnsignedInt PlutoSvgImporter::doImage2DCount() const {
    return 1;
}

Containers::Optional<ImageData2D> PlutoSvgImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* The alpha mode can be changed for every image import, so do the checking
       here and not in doOpenData(). Also doing that before anything else so
       people don't just wait ages for doomed-to-fail import with large
       files. */
    const Containers::StringView alphaMode = configuration().value<Containers::StringView>("alphaMode");
    if(alphaMode != ""_s && alphaMode != "premultipliedLinear"_s) {
        Error{} << "Trade::PlutoSvgImporter::image2D(): expected alphaMode to be either empty or premultipliedLinear but got" << alphaMode;
        return {};
    }

    /* Use the configuration-provided DPI value to scale the image. Similarly
       to ResvgImporter, one has to manually scale the document and then supply
       scaling via a matrix. I wonder which library got inspired from which. */
    const Float scaling = configuration().value<Float>("dpi")/96.0f;
    /* The rounding (and DPI being queried as a float) is verified in the
       load() test as well. */
    const Vector2i size{Int(Math::round(plutosvg_document_get_width(_state->document)*scaling)),
                        Int(Math::round(plutosvg_document_get_height(_state->document)*scaling))};

    /* Do an Y flip simply by specifying an Y-flipping transform in addition to
       the DPI scaling. Given that LunaSVG (and resvg) have a column-major 3x2
       matrix layout, I assume here it'll be the same.

        a c e   1 0 0
        b d f   0 1 0

       Thus compatible with Matrix3x2 and can be cast directly to it */
    const Matrix3 yFlip =
        Matrix3::translation(Vector2::yAxis(size.y()))*
        Matrix3::scaling({scaling, -scaling});
    plutovg_matrix matrix;
    Matrix3x2::from(&matrix.a) = Matrix3x2{yFlip};

    /** @todo expose rendering of subnodes? is it useful for anything? */

    /* Similar to LunaSVG, this is *rendering into* a bitmap, so the memory
       needs to be zero-initialized first, additionally therehas to be a canvas
       object created from a surface. */
    Containers::Array<char> data{ValueInit, std::size_t(size.product()*4)};
    plutovg_surface_t* surface = plutovg_surface_create_for_data(reinterpret_cast<std::uint8_t*>(data.data()), size.x(), size.y(), size.x()*4);
    Containers::ScopeGuard surfaceDestroy{surface, plutovg_surface_destroy};
    plutovg_canvas_t* canvas  = plutovg_canvas_create(surface);
    Containers::ScopeGuard canvasDestroy{canvas, plutovg_canvas_destroy};
    plutovg_canvas_transform(canvas, &matrix);
    /* The function returns false only if it cannot find the requested element
       ID, if passed. Since we pass nullptr, the function always succeeds. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(plutosvg_document_render(_state->document, nullptr, canvas, nullptr, nullptr, nullptr));

    /* PlutoSVG produces a premultiplied BGRA output, unfortunately (and same
       as with ResvgImporter and LunaSvgImporter) it doesn't correctly
       premultiply in sRGB. It provides an option to convert that to the usual
       unpremultiplied RGBA at least, which is nice. */
    if(alphaMode == ""_s)
        /** @todo maybe our own batch algorithm in Math/ColorBatch.h would be
            faster once it exists */
        plutovg_convert_argb_to_rgba(reinterpret_cast<std::uint8_t*>(data.data()), reinterpret_cast<std::uint8_t*>(data.data()), size.x(), size.y(), size.x()*4);
    else if(alphaMode == "premultipliedLinear"_s)
        /** @todo use a batch algorithm in Math/ColorBatch.h once it exists */
        for(Color4ub& i: Containers::arrayCast<Color4ub>(data))
            i = Math::gather<'b', 'g', 'r', 'a'>(i);
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    return ImageData2D{PixelFormat::RGBA8Unorm, size, Utility::move(data)};
}

}}

CORRADE_PLUGIN_REGISTER(PlutoSvgImporter, Magnum::Trade::PlutoSvgImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
