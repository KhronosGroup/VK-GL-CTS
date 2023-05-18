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
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"

// enable the define to disable protected memory
//#define NOT_PROTECTED	1

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

void								checkProtectedQueueSupport			(Context& context);

CustomInstance						makeProtectedMemInstance			(vkt::Context&						context,
																		 const std::vector<std::string>&	extraExtensions = std::vector<std::string>());
deUint32							chooseProtectedMemQueueFamilyIndex	(const vk::InstanceDriver&			vkd,
																		 vk::VkPhysicalDevice				physicalDevice,
																		 vk::VkSurfaceKHR					surface = DE_NULL);

vk::Move<vk::VkDevice>				makeProtectedMemDevice				(const vk::PlatformInterface&		vkp,
																		 vk::VkInstance						instance,
																		 const vk::InstanceDriver&			vkd,
																		 vk::VkPhysicalDevice				physicalDevice,
																		 const deUint32						queueFamilyIndex,
																		 const deUint32						apiVersion,
																		 const std::vector<std::string>&	extraExtensions,
#ifdef CTS_USES_VULKANSC
																		 de::SharedPtr<vk::ResourceInterface> resourceInterface,
#endif // CTS_USES_VULKANSC
																		 const tcu::CommandLine&			cmdLine);

vk::VkQueue							getProtectedQueue					(const vk::DeviceInterface&			vk,
																		 vk::VkDevice						device,
																		 const deUint32						queueFamilyIndex,
																		 const deUint32						queueIdx);

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

vk::VkResult						queueSubmit							(ProtectedContext&					context,
																		 ProtectionMode						protectionMode,
																		 vk::VkQueue						queue,
																		 vk::VkCommandBuffer				cmdBuffer,
																		 vk::VkFence						fence,
																		 deUint64							timeout);

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
																		 const vk::VkPrimitiveTopology		topology,
																		 const vk::VkPipelineCreateFlags	flags = 0);

void								clearImage							(ProtectedContext&					ctx,
																		 vk::VkImage						image);

void								uploadImage							(ProtectedContext&					ctx,
																		 vk::VkImage						image,
																		 const tcu::Texture2D&				texture2D);

void								copyToProtectedImage				(ProtectedContext&					ctx,
																		 vk::VkImage						srcImage,
																		 vk::VkImage						dstImage,
																		 vk::VkImageLayout					dstImageLayout,
																		 deUint32							width,
																		 deUint32							height,
																		 ProtectionMode						protectionMode = PROTECTION_ENABLED);

void								fillWithRandomColorTiles			(const tcu::PixelBufferAccess&		dst,
																		 const tcu::Vec4&					minVal,
																		 const tcu::Vec4&					maxVal,
																		 deUint32							seed);

void								fillWithUniqueColors				(const tcu::PixelBufferAccess&		dst,
																		 deUint32							seed);

} // ProtectedMem
} // vkt

#endif // _VKTPROTECTEDMEMUTILS_HPP
