#ifndef _VKYCBCRIMAGEWITHMEMORY_HPP
#define _VKYCBCRIMAGEWITHMEMORY_HPP
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
 * \brief YCbCr Image backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "deSharedPtr.hpp"

#include <vector>

namespace vk
{

typedef de::SharedPtr<Allocation>		AllocationSp;

class YCbCrImageWithMemory
{
public:
										YCbCrImageWithMemory	(const vk::DeviceInterface&		vk,
																 const vk::VkDevice				device,
																 vk::Allocator&					allocator,
																 const vk::VkImageCreateInfo&	imageCreateInfo,
																 const vk::MemoryRequirement	requirement);
	const vk::VkImage&					get						(void) const { return *m_image;			}
	const vk::VkImage&					operator*				(void) const { return get();			}
	const std::vector<AllocationSp>&	getAllocations			(void) const { return m_allocations;	}

private:
	const vk::Unique<vk::VkImage>		m_image;
	std::vector<AllocationSp>			m_allocations;

	// "deleted"
										YCbCrImageWithMemory	(const YCbCrImageWithMemory&);
	YCbCrImageWithMemory&				operator=				(const YCbCrImageWithMemory&);
};

} // vk

#endif // _VKYCBCRIMAGEWITHMEMORY_HPP
