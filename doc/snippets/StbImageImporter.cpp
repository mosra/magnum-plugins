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

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/System.h>
#include <Magnum/Trade/AbstractImporter.h>

using namespace Magnum;

/* GCC 11+ in Release warns that "this pointer is null". Yes. It is. Fuck off,
   those are documentation code snippets. */
#if defined(CORRADE_TARGET_GCC) && !defined(CORRADE_TARGET_CLANG) && __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Wnonnull"
#endif

int main() {
{
Containers::Pointer<Trade::AbstractImporter> importer;
/* [gif-delays] */
if(!importer->importerState())
    Fatal{} << "Not an animated GIF.";

Containers::ArrayView<const Int> frameDelays{
    reinterpret_cast<const Int*>(importer->importerState()),
    importer->image2DCount()};

for(UnsignedInt i = 0; i != importer->image2DCount(); ++i) {
    // display the image ...

    Utility::System::sleep(frameDelays[i]);
}
/* [gif-delays] */
}
}
