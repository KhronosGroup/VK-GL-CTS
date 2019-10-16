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
template<typename VectorT>
struct LessThan
{
	bool operator()(const VectorT& v1, const VectorT& v2) const
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

static inline deUint32 numSamplesPerPixel (const MultisamplePixelGrid& pixelGrid)
{
	return static_cast<deUint32>(pixelGrid.samplesPerPixel());
}

//! Fill each grid pixel with a distinct samples pattern, rounding locations based on subPixelBits
void fillSampleLocationsRandom (MultisamplePixelGrid& grid, const deUint32 subPixelBits, const deUint32 seed)
{
	const deUint32	guardOffset			= 1u;	// don't put samples on the right or the bottom edge of the pixel
	const deUint32	maxLocationIndex	= 1u << subPixelBits;
	de::Random		rng					(seed);

	for (deUint32 gridY = 0; gridY < grid.size().y(); ++gridY)
	for (deUint32 gridX = 0; gridX < grid.size().x(); ++gridX)
	{
		std::set<UVec2, LessThan<UVec2> >	takenLocationIndices;
		for (deUint32 sampleNdx = 0; sampleNdx < numSamplesPerPixel(grid); /* no increment */)
		{
			const UVec2 locationNdx (rng.getUint32() % (maxLocationIndex + 1 - guardOffset),
									 rng.getUint32() % (maxLocationIndex + 1 - guardOffset));

			if (takenLocationIndices.find(locationNdx) == takenLocationIndices.end())
			{
				const VkSampleLocationEXT location =
				{
					static_cast<float>(locationNdx.x()) / static_cast<float>(maxLocationIndex),	// float x;
					static_cast<float>(locationNdx.y()) / static_cast<float>(maxLocationIndex),	// float y;
				};

				grid.setSample(gridX, gridY, sampleNdx, location);
				takenLocationIndices.insert(locationNdx);

				++sampleNdx;	// next sample
			}
		}
	}
}

} // pipeline
} // vkt
