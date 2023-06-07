#ifndef Magnum_Implementation_formatVersion_h
#define Magnum_Implementation_formatVersion_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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

#include "Corrade/configure.h"

#ifndef CORRADE_IS_DEBUG_BUILD
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/version.h>
#include <Magnum/version.h>

#include "Magnum/versionPlugins.h"
#endif

/* Common code used by KtxImageConverter and GltfSceneConverter to record
   version string in generated files. Enabled only in release builds to not
   cause unnecessary rebuilds in developer working copies every time some
   Magnum commit changes. */
namespace Magnum { namespace Implementation { namespace {

#ifdef CORRADE_IS_DEBUG_BUILD
const char* formatPluginsVersion() { return "v<dev>"; }
#else
Containers::String formatPluginsVersion() {
    return Utility::format(
        /* Tag and then for each project number of commits since the tag and a
           unique hash prefixed with `g`. Commit info is present only in a full
           Git clone with all tags, it's omitted otherwise. Full example (with
           abbrev'd hashes):
            v2020.06-1341-g68d02-2187-gbd023-1097-gb7d34
           With only some commit info available:
            v2020.06-xxxx-2187-gbd023-xxxx
           With no commit info available:
            v2020.06 */
        "v{}.{:.2}"
        /* Commit & hash present only in a full Git clone with all
           tags, omit otherwise */
        #ifdef CORRADE_VERSION_COMMIT
        "-{}-g{:x}"
        #elif defined(MAGNUM_VERSION_COMMIT) || defined(MAGNUMPLUGINS_VERSION_COMMIT)
        "-xxxx"
        #endif
        #ifdef MAGNUM_VERSION_COMMIT
        "-{}-g{:x}"
        #elif defined(CORRADE_VERSION_COMMIT) || defined(MAGNUMPLUGINS_VERSION_COMMIT)
        "-xxxx"
        #endif
        #ifdef MAGNUMPLUGINS_VERSION_COMMIT
        "-{}-g{:x}"
        #elif defined(CORRADE_VERSION_COMMIT) || defined(MAGNUM_VERSION_COMMIT)
        "-xxxx"
        #endif
        /* The tag should be the same for all three */
        , CORRADE_VERSION_YEAR, CORRADE_VERSION_MONTH
        #ifdef CORRADE_VERSION_COMMIT
        , CORRADE_VERSION_COMMIT, CORRADE_VERSION_HASH
        #endif
        #ifdef MAGNUM_VERSION_COMMIT
        , MAGNUM_VERSION_COMMIT, MAGNUM_VERSION_HASH
        #endif
        #ifdef MAGNUMPLUGINS_VERSION_COMMIT
        , MAGNUMPLUGINS_VERSION_COMMIT, MAGNUMPLUGINS_VERSION_HASH
        #endif
    );
}
#endif

}}}

#endif
