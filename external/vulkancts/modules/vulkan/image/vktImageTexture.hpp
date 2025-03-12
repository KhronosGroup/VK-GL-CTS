#ifndef _VKTIMAGETEXTURE_HPP
#define _VKTIMAGETEXTURE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Texture utility class
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktImageTestsUtil.hpp"

namespace vkt
{
namespace image
{

//! Texture buffer/image abstraction. Helps managing size, number of layers and number of mipmap levels.
class Texture
{
public:
    Texture(const ImageType imageType, const tcu::IVec3 &imageLayerSize, const int layers, const int samples = 1,
            const int levels = 1);
    Texture(const Texture &other, const int samples);

    ImageType type(void) const
    {
        return m_type;
    }                                                   //!< Texture type
    tcu::IVec3 layerSize(const int mipLevel = 0) const; //!< Size of a single layer for mipmap level 0
    int numLayers(void) const
    {
        return m_numLayers;
    } //!< Number of array layers (for array and cube types)
    int numSamples(void) const
    {
        return m_numSamples;
    } //!< Number of samples per texel (multisampled texture)

    tcu::IVec3 size(const int mipLevel = 0)
        const; //!< Size including number of layers in additional dimension (e.g. z in 2d texture) for mipmap level 0
    int dimension(void) const; //!< Coordinate dimension used for addressing (e.g. 3 (x,y,z) for 2d array)
    int layerDimension(
        void) const; //!< Coordinate dimension used for addressing a single layer (e.g. 2 (x,y) for 2d array)

    int numMipmapLevels(void) const
    {
        return m_numMipmapLevels;
    } //!< Number of levels of detail (mipmap texture)

private:
    void checkInvariants(void) const;

    const tcu::IVec3 m_layerSize;
    const ImageType m_type;
    const int m_numLayers;
    const int m_numSamples;
    const int m_numMipmapLevels;
};

inline bool isCube(const Texture &texture)
{
    return texture.type() == IMAGE_TYPE_CUBE || texture.type() == IMAGE_TYPE_CUBE_ARRAY;
}

} // namespace image
} // namespace vkt

#endif // _VKTIMAGETEXTURE_HPP
