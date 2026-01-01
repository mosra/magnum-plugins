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

#include "ResvgImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Trade/ImageData.h>

#include <resvg.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

struct ResvgImporter::State {
    explicit State() {
        options = resvg_options_create();
    }

    ~State() {
        /* Lol, it doesn't accept null? */
        if(tree)
            resvg_tree_destroy(tree);
        resvg_options_destroy(options);
    }

    /* Disable copies and moves to avoid accidental double deletion */
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    Float dpi; /* set in doOpenFile() */
    resvg_options* options;
    resvg_render_tree* tree{};
};

ResvgImporter::ResvgImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin) : AbstractImporter{manager, plugin} {}

ResvgImporter::~ResvgImporter() = default;

ImporterFeatures ResvgImporter::doFeatures() const {
    return ImporterFeature::OpenData;
}

bool ResvgImporter::doIsOpened() const {
    /* If the state exists, the tree should too */
    CORRADE_INTERNAL_ASSERT(!_state || _state->tree);
    return !!_state;
}

void ResvgImporter::doClose() { _state = nullptr; }

void ResvgImporter::doOpenFile(Containers::StringView filename) {
    /* Save the base directory. Since we're slicing back the path from a
       filename, it's always non-null-terminated, but use nullTerminatedView()
       just out of habit.

       This is also just a "set and hope for the best" option, if an externally
       referenced image cannot be found, with or without the resource dir set,
       it just silently ignores it and we have no way to know that it did.
       Similarly, it's not possible to control or track this loading in any
       way, so file callbacks can't be used either. */
    _state.emplace();
    resvg_options_set_resources_dir(_state->options, Containers::String::nullTerminatedView(Utility::Path::path(filename)).data());

    /* delegate to the base implementation, which will delegate to
       doOpenData(). If it doesn't result in the tree being parsed (which can
       happen if for example the file doesn't exist at all), clear the
       state. */
    AbstractImporter::doOpenFile(filename);
    if(_state && !_state->tree)
        _state = nullptr;
}

void ResvgImporter::doOpenData(Containers::Array<char>&& data, DataFlags) {
    /* The state may have been created by doOpenFile() already */
    if(!_state)
        _state.emplace();

    /* Set the DPI. The configuration default matches Resvg default of 96.
       Funnily enough this *does not* affect the actual rendered image size,
       one has to do that separately via a transformation matrix when
       rendering. Not sure what this actually affects, so save the DPI for use
       in doImage2D() below so the two don't use something else when the DPI
       changes between the two. */
    _state->dpi = configuration().value<Float>("dpi");
    resvg_options_set_dpi(_state->options, _state->dpi);

    /* There's resvg_init_log() that, if called, prints messages to stderr. The
       problem is that we just cannot redirect stderr to anything on our side,
       which makes it not nice to use, but it could be at least made opt-in
       with ImporterFlag::Verbose or some such. Another problem is that it MUST
       BE CALLED ONLY ONCE, and there's no resvg_fini_log() or anything. Which,
       like ... did the developers ever try to *use* any library in a
       non-toy-project setting, besides making one? Such global state is just
       impossible to track, especially with dynamically-loaded plugins.
       SIGH. */

    /** @todo there's resvg_options_set_shape_rendering_mode(),
        resvg_options_set_text_rendering_mode(),
        resvg_options_set_image_rendering_mode(), defaults are optimizing for
        quality but maybe it might eventually be useful to produce a
        worse-looking output faster */

    const resvg_error error = resvg_error(resvg_parse_tree_from_data(data.data(), data.size(), _state->options, &_state->tree));
    if(error != RESVG_OK) {
        Error e;
        e << "Trade::ResvgImporter::openData():";
        switch(error) {
            case RESVG_ERROR_NOT_AN_UTF8_STR:
                e << "not an UTF-8 string";
                break;
            case RESVG_ERROR_MALFORMED_GZIP:
                e << "malformed GZip";
                break;
            case RESVG_ERROR_INVALID_SIZE:
                e << "invalid SVG size";
                break;
            case RESVG_ERROR_PARSING_FAILED:
                e << "parsing failed";
                break;
            /* LCOV_EXCL_START */
            /* Nope, this one doesn't fail if an external file fails to open,
               and thus since we only open data this gets never hit. See the
               externalImageNotFound() test for details. */
            case RESVG_ERROR_FILE_OPEN_FAILED:
            /* This one is exposed in the C API, but the underlying Rust
               ElementsLimitReached seems to not be used anywhere in the code
               apart from trivial enum translation, so I assume the code that
               originated in https://github.com/linebender/resvg/commit/454b4dbab53058047b715e3cfdc0808d2287155b
               is no longer there now. See the tooLarge() test for a repro
               case. */
            case RESVG_ERROR_ELEMENTS_LIMIT_REACHED:
            case RESVG_OK:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            /* LCOV_EXCL_STOP */
        }

        /* Rest the state and exit */
        _state = nullptr;
        return;
    }

    /* Everything is okay. Well, or ... could be. If an externally referenced
       image isn't found, the import doesn't fail, and even if it is found the
       import doesn't always give back correct output. See the
       externalImage() / externalImageNotFound() test and associated files for
       a repro case. The resvg_is_image_empty() isn't useful for failure
       detection either.

       There's also a whole logic around font loading. But, diven the above, do
       I even want to dive into that? */
}

UnsignedInt ResvgImporter::doImage2DCount() const {
    return 1;
}

Containers::Optional<ImageData2D> ResvgImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* The alpha mode can be changed for every image import, so do the checking
       here and not in doOpenData(). Also doing that before anything else so
       people don't just wait ages for doomed-to-fail import with large
       files. */
    const Containers::StringView alphaMode = configuration().value<Containers::StringView>("alphaMode");
    if(alphaMode != ""_s &&
        alphaMode != "premultipliedLinear"_s) {
        Error{} << "Trade::ResvgImporter::image2D(): expected alphaMode to be either empty or premultipliedLinear but got" << alphaMode;
        return {};
    }

    /* Apparently resvg_options_set_dpi() doesn't actually affect the output
       size (while setting DPI in Inkscape output does, which seems like a good
       standard to match), so multiply the size by the ratio to the default 96
       DPI and use that in the transformation below. Not querying
       `configuration().value<Float>("dpi")` again as I'm not sure what
       resvg_options_set_dpi() actually does and whether it affects also
       resvg_parse_tree_from_data() or only resvg_render() -- thus it's set
       in doOpenData() above already and the DPI value is cached to ensure the
       same value is used for parsing and for rendering. */
    const Float scaling = _state->dpi/96.0f;
    const resvg_size sizeF = resvg_get_image_size(_state->tree);
    /* The rounding (and DPI being queried as a float) is verified in the
       load() test as well. */
    const Vector2i size{Int(Math::round(sizeF.width*scaling)),
                        Int(Math::round(sizeF.height*scaling))};

    /* Do an Y flip simply by specifying an Y-flipping transform in addition to
       the DPI scaling. Total guesswork but based on resvg_transform_identity()
       returning 100100 I assume the matrix layout is the following, i.e.
       column-major:

        a c e   1 0 0
        b d f   0 1 0

       Thus compatible with Matrix3x2 and can be cast directly to it. */
    const Matrix3 yFlip =
        Matrix3::translation(Vector2::yAxis(size.y()))*
        Matrix3::scaling({scaling, -scaling});
    resvg_transform transform;
    Matrix3x2::from(&transform.a) = Matrix3x2{yFlip};

    /** @todo expose rendering of subnodes? is it useful for anything? */

    /* Useless behavior -- resvg_render() renders *onto* a bitmap, i.e. not
       just writing to it but blending there. Thus the output memory has to be
       zero-initialized and then the rendering operation does extra work that
       cannot be turned off and could be better done by something else. Could
       use it to specify background color for example, but then there's the
       problem with incorrect premultiplication below (and thus likely
       incorrect sRGB handling when blending as well) so ... */
    /** @todo expose the background color option nevertheless? */
    Containers::Array<char> data{ValueInit, std::size_t(size.product()*4)};
    resvg_render(_state->tree, transform, size.x(), size.y(), data.data());

    /* Resvg produces a premultiplied output, unfortunately it doesn't
       correctly premultiply in sRGB. So when one wants to do the
       premultiplication properly, it has to be undone first, which means we
       just unpremultiply always and then let the user code freely decide what
       to do, whether premultiply correctly, not premultiply at all or
       premultiply incorrectly.
        https://github.com/linebender/resvg/issues/839 */
    if(alphaMode == ""_s)
        /** @todo use a batch algorithm in Math/ColorBatch.h once it exists */
        for(Color4ub& i: Containers::arrayCast<Color4ub>(data))
            i = i.unpremultiplied();
    else CORRADE_INTERNAL_ASSERT(alphaMode == "premultipliedLinear"_s);

    return ImageData2D{PixelFormat::RGBA8Unorm, size, Utility::move(data)};
}

}}

CORRADE_PLUGIN_REGISTER(ResvgImporter, Magnum::Trade::ResvgImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
