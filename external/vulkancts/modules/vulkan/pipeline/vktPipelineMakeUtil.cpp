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
 * \brief Object creation utilities
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMakeUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{
using namespace vk;
using de::MovePtr;

Buffer::Buffer (const vk::DeviceInterface&		vk,
				const vk::VkDevice				device,
				vk::Allocator&					allocator,
				const vk::VkBufferCreateInfo&	bufferCreateInfo,
				const vk::MemoryRequirement		memoryRequirement)
	: m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
	, m_allocation	(bindBuffer(vk, device, allocator, *m_buffer, memoryRequirement))
{
}

Image::Image (const vk::DeviceInterface&		vk,
			  const vk::VkDevice				device,
			  vk::Allocator&					allocator,
			  const vk::VkImageCreateInfo&		imageCreateInfo,
			  const vk::MemoryRequirement		memoryRequirement)
	: m_image		(createImage(vk, device, &imageCreateInfo))
	, m_allocation	(bindImage(vk, device, allocator, *m_image, memoryRequirement))
{
}

Move<VkCommandBuffer> makeCommandBuffer (const DeviceInterface& vk, const VkDevice device, const VkCommandPool commandPool)
{
	return allocateCommandBuffer(vk, device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule,
									  const VkSpecializationInfo*	specInfo)
{
	const VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		specInfo,												// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags			flags;
		shaderStageInfo,									// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineInfo);
}

MovePtr<Allocation> bindImageDedicated (const InstanceInterface& vki, const DeviceInterface& vkd, const VkPhysicalDevice physDevice, const VkDevice device, const VkImage image, const MemoryRequirement requirement)
{
	MovePtr<Allocation> alloc(allocateDedicated(vki, vkd, physDevice, device, image, requirement));
	VK_CHECK(vkd.bindImageMemory(device, image, alloc->getMemory(), alloc->getOffset()));
	return alloc;
}

MovePtr<Allocation> bindBufferDedicated (const InstanceInterface& vki, const DeviceInterface& vkd, const VkPhysicalDevice physDevice, const VkDevice device, const VkBuffer buffer, const MemoryRequirement requirement)
{
	MovePtr<Allocation> alloc(allocateDedicated(vki, vkd, physDevice, device, buffer, requirement));
	VK_CHECK(vkd.bindBufferMemory(device, buffer, alloc->getMemory(), alloc->getOffset()));
	return alloc;
}

} // pipeline
} // vkt
