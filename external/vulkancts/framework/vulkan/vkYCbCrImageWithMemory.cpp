/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief YCbCR Image backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkYCbCrImageWithMemory.hpp"

#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"

namespace vk
{

YCbCrImageWithMemory::YCbCrImageWithMemory (const vk::DeviceInterface&		vk,
											const vk::VkDevice				device,
											vk::Allocator&					allocator,
											const vk::VkImageCreateInfo&	imageCreateInfo,
											const vk::MemoryRequirement		requirement)
	: m_image	(createImage(vk, device, &imageCreateInfo))
{
	if ((imageCreateInfo.flags & VK_IMAGE_CREATE_DISJOINT_BIT_KHR) != 0)
	{
		const deUint32	numPlanes	= getPlaneCount(imageCreateInfo.format);
		for (deUint32 planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
		{
			const VkImageAspectFlagBits	planeAspect	= getPlaneAspect(planeNdx);
			const VkMemoryRequirements	reqs		= getImagePlaneMemoryRequirements(vk, device, *m_image, planeAspect);

			m_allocations.push_back(AllocationSp(allocator.allocate(reqs, requirement).release()));

			bindImagePlaneMemory(vk, device, *m_image, m_allocations.back()->getMemory(), m_allocations.back()->getOffset(), planeAspect);
		}
	}
	else
	{
		const VkMemoryRequirements reqs = getImageMemoryRequirements(vk, device, *m_image);
		m_allocations.push_back(AllocationSp(allocator.allocate(reqs, requirement).release()));
		VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocations.back()->getMemory(), m_allocations.back()->getOffset()));
	}
}

} // vk
