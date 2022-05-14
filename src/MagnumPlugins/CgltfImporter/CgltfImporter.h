#ifndef Magnum_Trade_CgltfImporter_h
#define Magnum_Trade_CgltfImporter_h
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

#ifdef MAGNUM_BUILD_DEPRECATED
/** @file
 * @brief Class @ref Magnum::Trade::CgltfImporter
 * @m_deprecated_since_latest Use @ref MagnumPlugins/GltfImporter/GltfImporter.h
 *      and the @relativeref{Magnum,Trade::GltfImporter} class instead.
 */
#endif

#include <Magnum/configure.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include "MagnumPlugins/GltfImporter/GltfImporter.h"

#ifndef _MAGNUM_NO_DEPRECATED_CGLTFIMPORTER
CORRADE_DEPRECATED_FILE("use MagnumPlugins/GltfImporter/GltfImporter.h and the GltfImporter class instead")
#endif

namespace Magnum { namespace Trade {

/**
@brief glTF importer plugin
@m_deprecated_since_latest Use the @ref GltfImporter plugin instead --- this
    plugin is now just a differently named wrapper over it.

@m_keywords{GltfImporter}

This plugins provides the `GltfImporter` plugin.
*/
#ifdef DOXYGEN_GENERATING_OUTPUT
class CgltfImporter: public GltfImporter {};
#else
typedef CORRADE_DEPRECATED("use GltfImporter instead") GltfImporter CgltfImporter;
#endif

}}
#else
#error use MagnumPlugins/GltfImporter/GltfImporter.h and the GltfImporter class instead
#endif

#endif
