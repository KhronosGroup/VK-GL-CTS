#ifndef _VKTPROTECTEDMEMUTILS_HPP
#define _VKTPROTECTEDMEMUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
 * \brief Protected Memory Utility methods
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "vktTestCase.hpp"
#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuVector.hpp"

namespace vkt
{
namespace ProtectedMem
{

class ProtectedContext;

enum ProtectionMode {
	PROTECTION_DISABLED	= 0,
	PROTECTION_ENABLED	= 1,
};

typedef std::vector<vk::VkVertexInputBindingDescription>	VertexBindings;
typedef std::vector<vk::VkVertexInputAttributeDescription>	VertexAttribs;

vk::Move<vk::VkInstance>			makeProtectedMemInstance			(const vk::PlatformInterface&		vkp,
																		 bool								isValidationEnabled);
deUint32							chooseProtectedMemQueueFamilyIndex	(const vk::InstanceDriver&			vkd,
																		 vk::VkPhysicalDevice				physicalDevice);

vk::Move<vk::VkDevice>				makeProtectedMemDevice				(const vk::InstanceDriver&			vkd,
																		 vk::VkPhysicalDevice				physicalDevice,
																		 const deUint32						queueFamilyIndex);


de::MovePtr<vk::ImageWithMemory>	createImage2D						(ProtectedContext&					context,
																		 ProtectionMode						protectionMode,
																		 const deUint32						queueFamilyIdx,
																		 deUint32							width,
																		 deUint32							height,
																		 vk::VkFormat						format,
																		 vk::VkImageUsageFlags				usageFlags);
de::MovePtr<vk::BufferWithMemory>	makeBuffer							(ProtectedContext&					context,
																		 ProtectionMode						protectionMode,
																		 const deUint32						queueFamilyIdx,
																		 deUint32							size,
																		 vk::VkBufferUsageFlags				flags,
																		 vk::MemoryRequirement				memReq);

vk::Move<vk::VkImageView>			createImageView						(ProtectedContext&					context,
																		 vk::VkImage						image,
																		 vk::VkFormat						format);
vk::Move<vk::VkRenderPass>			createRenderPass					(ProtectedContext&					context,
																		 vk::VkFormat						format);
vk::Move<vk::VkFramebuffer>			createFramebuffer					(ProtectedContext&					context,
																		 deUint32							width,
																		 deUint32							height,
																		 vk::VkRenderPass					renderPass,
																		 vk::VkImageView					colorImageView);
vk::Move<vk::VkPipelineLayout>		createPipelineLayout				(ProtectedContext&					context,
																		 deUint32							layoutCount,
																		 vk::VkDescriptorSetLayout*			setLayouts);

typedef vk::VkCommandBufferInheritanceInfo CmdBuffInheritance;
enum CmdBufferType {
	CMD_BUFFER_PRIMARY,
	CMD_BUFFER_SECONDARY,
};

const char*							getCmdBufferTypeStr					(const CmdBufferType				cmdBufferType);
void								beginSecondaryCommandBuffer			(const vk::DeviceInterface&			vk,
																		 const vk::VkCommandBuffer			secondaryCmdBuffer,
																		 const CmdBuffInheritance			secCmdBufInheritInfo);

void								beginCommandBuffer					(const vk::DeviceInterface&			vk,
																		 const vk::VkCommandBuffer			commandBuffer);
vk::VkResult						queueSubmit							(ProtectedContext&					context,
																		 ProtectionMode						protectionMode,
																		 vk::VkQueue						queue,
																		 vk::VkCommandBuffer				cmdBuffer,
																		 vk::VkFence						fence,
																		 uint64_t							timeout);

vk::Move<vk::VkDescriptorSet>		makeDescriptorSet					(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice					device,
																		 const vk::VkDescriptorPool			descriptorPool,
																		 const vk::VkDescriptorSetLayout	setLayout);
vk::Move<vk::VkPipelineLayout>		makePipelineLayout					(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice					device,
																		 const vk::VkDescriptorSetLayout	descriptorSetLayout);

vk::Move<vk::VkPipeline>			makeComputePipeline					(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice					device,
																		 const vk::VkPipelineLayout			pipelineLayout,
																		 const vk::VkShaderModule			shaderModule,
																		 const vk::VkSpecializationInfo*	specInfo);

vk::Move<vk::VkSampler>				makeSampler							(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice&				device);
vk::Move<vk::VkCommandPool>			makeCommandPool						(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice&				device,
																		 ProtectionMode						protectionMode,
																		 const deUint32						queueFamilyIdx);

vk::Move<vk::VkPipeline>			makeGraphicsPipeline				(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice					device,
																		 const vk::VkPipelineLayout			pipelineLayout,
																		 const vk::VkRenderPass				renderPass,
																		 const vk::VkShaderModule			vertexShaderModule,
																		 const vk::VkShaderModule			fragmentShaderModule,
																		 const VertexBindings&				vertexBindings,
																		 const VertexAttribs&				vertexAttribs,
																		 const tcu::UVec2&					renderSize,
																		 const vk::VkPrimitiveTopology		topology);

vk::Move<vk::VkPipeline>			makeGraphicsPipeline				(const vk::DeviceInterface&			vk,
																		 const vk::VkDevice					device,
																		 const vk::VkPipelineLayout			pipelineLayout,
																		 const vk::VkRenderPass				renderPass,
																		 const vk::VkShaderModule			vertexShaderModule,
																		 const vk::VkShaderModule			fragmentShaderModule,
																		 const VertexBindings&				vertexBindings,
																		 const VertexAttribs&				vertexAttribs,
																		 const tcu::UVec2&					renderSize,
																		 const vk::VkPrimitiveTopology		topology);

} // ProtectedMem
} // vkt

#endif // _VKTPROTECTEDMEMUTILS_HPP
