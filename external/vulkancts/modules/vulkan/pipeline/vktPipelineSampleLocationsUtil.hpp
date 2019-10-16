#ifndef _VKTPIPELINESAMPLELOCATIONSUTIL_HPP
#define _VKTPIPELINESAMPLELOCATIONSUTIL_HPP
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

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "tcuVector.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{

//! Specify sample locations in a pixel grid
class MultisamplePixelGrid
{
public:
	MultisamplePixelGrid (const tcu::UVec2& gridSize, const vk::VkSampleCountFlagBits numSamples)
		: m_gridSize		(gridSize)
		, m_numSamples		(numSamples)
		, m_sampleLocations	(gridSize.x() * gridSize.y() * numSamples)
	{
		DE_ASSERT(gridSize.x() > 0 && gridSize.y() > 0);
		DE_ASSERT(numSamples   > 1);
	}

	//! If grid x,y is larger than gridSize, then each coordinate is wrapped, x' = x % size_x
	const vk::VkSampleLocationEXT& getSample (deUint32 gridX, deUint32 gridY, const deUint32 sampleNdx) const
	{
		return m_sampleLocations[getSampleIndex(gridX, gridY, sampleNdx)];
	}

	void setSample (const deUint32 gridX, const deUint32 gridY, const deUint32 sampleNdx, const vk::VkSampleLocationEXT& location)
	{
		DE_ASSERT(gridX < m_gridSize.x());
		DE_ASSERT(gridY < m_gridSize.y());

		m_sampleLocations[getSampleIndex(gridX, gridY, sampleNdx)] = location;
	}

	const tcu::UVec2&				size				(void) const	{ return m_gridSize; }
	vk::VkSampleCountFlagBits		samplesPerPixel		(void) const	{ return m_numSamples; }
	const vk::VkSampleLocationEXT*	sampleLocations		(void) const	{ return dataOrNullPtr(m_sampleLocations); }
	vk::VkSampleLocationEXT*		sampleLocations		(void)			{ return dataOrNullPtr(m_sampleLocations); }
	deUint32						sampleLocationCount	(void) const	{ return static_cast<deUint32>(m_sampleLocations.size()); }

private:
	deUint32 getSampleIndex (deUint32 gridX, deUint32 gridY, const deUint32 sampleNdx) const
	{
		gridX %= m_gridSize.x();
		gridY %= m_gridSize.y();
		return (gridY * m_gridSize.x() + gridX) * static_cast<deUint32>(m_numSamples) + sampleNdx;
	}

	tcu::UVec2								m_gridSize;
	vk::VkSampleCountFlagBits				m_numSamples;
	std::vector<vk::VkSampleLocationEXT>	m_sampleLocations;
};

//! References the data inside MultisamplePixelGrid
inline vk::VkSampleLocationsInfoEXT makeSampleLocationsInfo (const MultisamplePixelGrid& pixelGrid)
{
	const vk::VkSampleLocationsInfoEXT info =
	{
		vk::VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,				// VkStructureType               sType;
		DE_NULL,														// const void*                   pNext;
		pixelGrid.samplesPerPixel(),									// VkSampleCountFlagBits         sampleLocationsPerPixel;
		vk::makeExtent2D(pixelGrid.size().x(), pixelGrid.size().y()),	// VkExtent2D                    sampleLocationGridSize;
		pixelGrid.sampleLocationCount(),								// uint32_t                      sampleLocationsCount;
		pixelGrid.sampleLocations(),									// const VkSampleLocationEXT*    pSampleLocations;
	};
	return info;
}

//! Fill each grid pixel with a distinct samples pattern, rounding locations based on subPixelBits
void fillSampleLocationsRandom (MultisamplePixelGrid& grid, const deUint32 subPixelBits, const deUint32 seed = 142u);

} // pipeline
} // vkt

#endif // _VKTPIPELINESAMPLELOCATIONSUTIL_HPP
