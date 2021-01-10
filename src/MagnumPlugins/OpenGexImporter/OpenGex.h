/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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

/** @file
 * @brief Namespace @ref Magnum::Trade::OpenGex
 */

#include "Magnum/Types.h"

namespace Magnum { namespace Trade {

/** @namespace Magnum::Trade::OpenGex
@brief OpenGEX structure and property names

Use these identifiers to access the document tree using the @ref OpenDdl
parser.

This library is built if `WITH_OPENGEXIMPORTER` is enabled when building Magnum
Plugins. To use this library with CMake, request the `OpenGexImporter`
component of the `MagnumPlugins` package and link to the
`MagnumPlugins::OpenGexImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED OpenGexImporter)

# ...
target_link_libraries(your-app MagnumPlugins::OpenGexImporter)
@endcode

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following *before* calling @cmake find_package() @ce:

@code{.cmake}
set(WITH_OPENGEXIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)
@endcode
*/
namespace OpenGex {

enum: Int {
    Animation,          /**< `Animation` structure */
    Atten,              /**< `Atten` structure */
    BoneCountArray,     /**< `BoneCountArray` structure */
    BoneIndexArray,     /**< `BoneIndexArray` structure */
    BoneNode,           /**< `BoneNode` structure */
    BoneRefArray,       /**< `BoneRefArray` structure */
    BoneWeightArray,    /**< `BoneWeightArray` structure */
    CameraNode,         /**< `CameraNode` structure */
    CameraObject,       /**< `CameraObject` structure */
    Clip,               /**< `Clip` structure */
    Color,              /**< `Color` structure */
    Extension,          /**< `Extension` structure */
    GeometryNode,       /**< `GeometryNode` structure */
    GeometryObject,     /**< `GeometryObject` structure */
    IndexArray,         /**< `IndexArray` structure */
    Key,                /**< `Key` structure */
    LightNode,          /**< `LightNode` structure */
    LightObject,        /**< `LightObject` structure */
    Material,           /**< `Material` structure */
    MaterialRef,        /**< `MaterialRef` structure */
    Mesh,               /**< `Mesh` structure */
    Metric,             /**< `Metrix` structure */
    Morph,              /**< `Morph` structure */
    MorphWeight,        /**< `MorphWeight` structure */
    Name,               /**< `Name` structure */
    Node,               /**< `Node` structure */
    ObjectRef,          /**< `ObjectRef` structure */
    Param,              /**< `Param` structure */
    Rotation,           /**< `Rotation` structure */
    Scale,              /**< `Scale` structure */
    Skeleton,           /**< `Skeleton` structure */
    Skin,               /**< `Skin` structure */
    Texture,            /**< `Texture` structure */
    Time,               /**< `Time` structure */
    Track,              /**< `Track` structure */
    Transform,          /**< `Transform` structure */
    Translation,        /**< `Translation` structure */
    Value,              /**< `Value` structure */
    VertexArray         /**< `VertexArray` structure */
};

enum: Int {
    applic,             /**< `applic` property */
    attrib,             /**< `attrib` property */
    begin,              /**< `begin` property */
    clip,               /**< `clip` property */
    curve,              /**< `curve` property */
    end,                /**< `end` property */
    front,              /**< `front` property */
    index,              /**< `index` property */
    key,                /**< `key` property */
    kind,               /**< `kind` property */
    lod,                /**< `lod` property */
    material,           /**< `material` property */
    morph,              /**< `morph` property */
    motion_blur,        /**< `motion_blur` property */
    object,             /**< `object` property */
    primitive,          /**< `primitive` property */
    restart,            /**< `restart` property */
    shadow,             /**< `shadow` property */
    target,             /**< `target` property */
    texcoord,           /**< `texcoord` property */
    two_sided,          /**< `two_sided` property */
    type,               /**< `type` property */
    visible             /**< `visible` property */
};

}}}
