/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Utilities for VK_EXT_sample_locations
 *//*--------------------------------------------------------------------*/

#include "vktPipelineSampleLocationsUtil.hpp"
#include "deRandom.hpp"
#include <set>

namespace vkt
{
namespace pipeline
{
using namespace vk;
using tcu::UVec2;
using tcu::Vec2;

//! Order a Vector by X, Y, Z, and W
template <typename VectorT>
struct LessThan
{
    bool operator()(const VectorT &v1, const VectorT &v2) const
    {
        for (int i = 0; i < VectorT::SIZE; ++i)
        {
            if (v1[i] == v2[i])
                continue;
            else
                return v1[i] < v2[i];
        }

        return false;
    }
};

static inline uint32_t numSamplesPerPixel(const MultisamplePixelGrid &pixelGrid)
{
    return static_cast<uint32_t>(pixelGrid.samplesPerPixel());
}

//! Fill each grid pixel with a distinct samples pattern, rounding locations based on subPixelBits
void fillSampleLocationsRandom(MultisamplePixelGrid &grid, const uint32_t subPixelBits, const uint32_t seed)
{
    const uint32_t guardOffset      = 1u; // don't put samples on the right or the bottom edge of the pixel
    const uint32_t maxLocationIndex = 1u << subPixelBits;
    de::Random rng(seed);

    for (uint32_t gridY = 0; gridY < grid.size().y(); ++gridY)
        for (uint32_t gridX = 0; gridX < grid.size().x(); ++gridX)
        {
            std::set<UVec2, LessThan<UVec2>> takenLocationIndices;
            for (uint32_t sampleNdx = 0; sampleNdx < numSamplesPerPixel(grid); /* no increment */)
            {
                const UVec2 locationNdx(rng.getUint32() % (maxLocationIndex + 1 - guardOffset),
                                        rng.getUint32() % (maxLocationIndex + 1 - guardOffset));

                if (takenLocationIndices.find(locationNdx) == takenLocationIndices.end())
                {
                    const VkSampleLocationEXT location = {
                        static_cast<float>(locationNdx.x()) / static_cast<float>(maxLocationIndex), // float x;
                        static_cast<float>(locationNdx.y()) / static_cast<float>(maxLocationIndex), // float y;
                    };

                    grid.setSample(gridX, gridY, sampleNdx, location);
                    takenLocationIndices.insert(locationNdx);

                    ++sampleNdx; // next sample
                }
            }
        }
}

} // namespace pipeline
} // namespace vkt
