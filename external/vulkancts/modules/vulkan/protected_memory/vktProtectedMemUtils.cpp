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

#include "vktProtectedMemUtils.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deString.h"
#include "deRandom.hpp"

#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkObjUtil.hpp"

#include "vkPlatform.hpp"
#include "vktProtectedMemContext.hpp"
#include "vkWsiUtil.hpp"
#include "vkObjUtil.hpp"

namespace vkt
{

using namespace vk;

namespace ProtectedMem
{

typedef std::vector<vk::VkExtensionProperties> Extensions;

CustomInstance makeProtectedMemInstance (vkt::Context& context, const std::vector<std::string>& extraExtensions)
{
	const PlatformInterface&	vkp = context.getPlatformInterface();
	const Extensions			supportedExtensions(vk::enumerateInstanceExtensionProperties(vkp, DE_NULL));
	std::vector<std::string>	requiredExtensions = extraExtensions;

	deUint32 apiVersion = context.getUsedApiVersion();
	if (!isCoreInstanceExtension(apiVersion, "VK_KHR_get_physical_device_properties2"))
		requiredExtensions.push_back("VK_KHR_get_physical_device_properties2");

	// extract extension names
	std::vector<std::string> extensions;
	for (const auto& e : supportedExtensions)
		extensions.push_back(e.extensionName);

	for (const auto& extName : requiredExtensions)
	{
		if (!isInstanceExtensionSupported(apiVersion, extensions, extName))
			TCU_THROW(NotSupportedError, (extName + " is not supported").c_str());
	}

	return createCustomInstanceWithExtensions(context, requiredExtensions);
}

void checkProtectedQueueSupport (Context& context)
{
#ifdef NOT_PROTECTED
	return;
#endif

	const vk::InstanceInterface&				vkd				= context.getInstanceInterface();
	vk::VkPhysicalDevice						physDevice		= context.getPhysicalDevice();
	std::vector<vk::VkQueueFamilyProperties>	properties;
	deUint32									numFamilies		= 0;

	vkd.getPhysicalDeviceQueueFamilyProperties(physDevice, &numFamilies, DE_NULL);
	DE_ASSERT(numFamilies > 0);
	properties.resize(numFamilies);

	vkd.getPhysicalDeviceQueueFamilyProperties(physDevice, &numFamilies, properties.data());

	for (auto prop: properties)
		if (prop.queueFlags & vk::VK_QUEUE_PROTECTED_BIT)
			return;

	TCU_THROW(NotSupportedError, "No protected queue found.");
}

deUint32 chooseProtectedMemQueueFamilyIndex	(const vk::InstanceDriver&	vkd,
											 vk::VkPhysicalDevice		physicalDevice,
											 vk::VkSurfaceKHR			surface)
{
	std::vector<vk::VkQueueFamilyProperties>	properties;
	deUint32									numFamilies		= 0;

	vkd.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numFamilies, DE_NULL);
	DE_ASSERT(numFamilies > 0);
	properties.resize(numFamilies);

	vkd.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numFamilies, properties.data());

	// Get a universal protected queue family index
	vk::VkQueueFlags	requiredFlags = vk::VK_QUEUE_GRAPHICS_BIT
										| vk::VK_QUEUE_COMPUTE_BIT
#ifndef NOT_PROTECTED
										| vk::VK_QUEUE_PROTECTED_BIT
#endif
										;
	for (size_t idx = 0; idx < properties.size(); ++idx)
	{
		vk::VkQueueFlags	flags = properties[idx].queueFlags;

		if (surface != DE_NULL
			&& vk::wsi::getPhysicalDeviceSurfaceSupport(vkd, physicalDevice, (deUint32)idx, surface) == VK_FALSE)
			continue; // Skip the queue family index if it does not support the surface

		if ((flags & requiredFlags) == requiredFlags)
			return (deUint32)idx;
	}

	TCU_THROW(NotSupportedError, "No matching universal protected queue found");
}

vk::Move<vk::VkDevice> makeProtectedMemDevice	(const vk::PlatformInterface&		vkp,
												 vk::VkInstance						instance,
												 const vk::InstanceDriver&			vkd,
												 vk::VkPhysicalDevice				physicalDevice,
												 const deUint32						queueFamilyIndex,
												 const deUint32						apiVersion,
												 const std::vector<std::string>&	extraExtensions,
												 bool								validationEnabled)
{
	const Extensions					supportedExtensions	(vk::enumerateDeviceExtensionProperties(vkd, physicalDevice, DE_NULL));
	std::vector<std::string>			requiredExtensions;
	std::vector<std::string>			extensions			= extraExtensions;

	if (apiVersion < VK_API_VERSION_1_1)
		TCU_THROW(NotSupportedError, "Vulkan 1.1 is not supported");

	bool								useYCbCr			= de::contains(extensions.begin(), extensions.end(), std::string("VK_KHR_sampler_ycbcr_conversion"));

	// Check if the physical device supports the protected memory extension name
	for (deUint32 ndx = 0; ndx < extensions.size(); ++ndx)
	{
		bool notInCore = !isCoreDeviceExtension(apiVersion, extensions[ndx]);
		if (notInCore && !isExtensionSupported(supportedExtensions.begin(), supportedExtensions.end(), RequiredExtension(extensions[ndx])))
			TCU_THROW(NotSupportedError, (extensions[ndx] + " is not supported").c_str());

		if (notInCore)
			requiredExtensions.push_back(extensions[ndx]);
	}

	std::vector<const char*>			enabledExts			(requiredExtensions.size());
	for (size_t idx = 0; idx < requiredExtensions.size(); ++idx)
	{
		enabledExts[idx] = requiredExtensions[idx].c_str();
	}

	vk::VkPhysicalDeviceSamplerYcbcrConversionFeatures		ycbcrFeature	=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
		DE_NULL,
		VK_FALSE
	};
	// Check if the protected memory can be enabled on the physical device.
	vk::VkPhysicalDeviceProtectedMemoryFeatures	protectedFeature =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,	// sType
		&ycbcrFeature,														// pNext
		VK_FALSE															// protectedMemory
	};
	vk::VkPhysicalDeviceFeatures					features;
	deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));

	vk::VkPhysicalDeviceFeatures2				featuresExt		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,					// sType
		&protectedFeature,													// pNext
		features
	};

	vkd.getPhysicalDeviceFeatures2(physicalDevice, &featuresExt);

#ifndef NOT_PROTECTED
	if (protectedFeature.protectedMemory == VK_FALSE)
		TCU_THROW(NotSupportedError, "Protected Memory feature not supported by the device");
#endif

	if (useYCbCr && !ycbcrFeature.samplerYcbcrConversion)
		TCU_THROW(NotSupportedError, "VK_KHR_sampler_ycbcr_conversion is not supported");

	const float							queuePriorities[]	= { 1.0f };
	const vk::VkDeviceQueueCreateInfo	queueInfos[]		=
	{
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
#ifndef NOT_PROTECTED
			(vk::VkDeviceQueueCreateFlags)vk::VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,
#else
			(vk::VkDeviceQueueCreateFlags)0u,
#endif
			queueFamilyIndex,
			DE_LENGTH_OF_ARRAY(queuePriorities),
			queuePriorities
		}
	};

	const vk::VkDeviceCreateInfo		deviceParams		=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,						// sType
		&featuresExt,													// pNext
		(vk::VkDeviceCreateFlags)0,										// flags
		DE_LENGTH_OF_ARRAY(queueInfos),									// queueCreateInfosCount
		&queueInfos[0],													// pQueueCreateInfos
		0u,																// enabledLayerCount
		DE_NULL,														// pEnabledLayerNames
		(deUint32)requiredExtensions.size(),							// enabledExtensionCount
		requiredExtensions.empty() ? DE_NULL : &enabledExts[0],			// pEnabledExtensionNames
		DE_NULL															// pEnabledFeatures
	};

	return createCustomDevice(validationEnabled, vkp, instance, vkd, physicalDevice, &deviceParams, DE_NULL);
}

vk::VkQueue getProtectedQueue	(const vk::DeviceInterface&	vk,
								 vk::VkDevice				device,
								 const deUint32				queueFamilyIndex,
								 const deUint32				queueIdx)
{
	const vk::VkDeviceQueueInfo2	queueInfo	=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,		// sType
		DE_NULL,										// pNext
		vk::VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,		// flags
		queueFamilyIndex,								// queueFamilyIndex
		queueIdx,										// queueIndex
	};

	(void)queueInfo;
	vk::VkQueue						queue		=
#ifndef NOT_PROTECTED
													vk::getDeviceQueue2(vk, device, &queueInfo);
#else
													vk::getDeviceQueue(vk, device, queueFamilyIndex, 0);
#endif

	if (queue == DE_NULL)
		TCU_THROW(TestError, "Unable to get a protected queue");

	return queue;
}

de::MovePtr<vk::ImageWithMemory>	createImage2D		(ProtectedContext&		context,
														 ProtectionMode			protectionMode,
														 const deUint32			queueFamilyIdx,
														 deUint32				width,
														 deUint32				height,
														 vk::VkFormat			format,
														 vk::VkImageUsageFlags	usageFlags)
{
	const vk::DeviceInterface&	vk			= context.getDeviceInterface();
	const vk::VkDevice&			device		= context.getDevice();
	vk::Allocator&				allocator	= context.getDefaultAllocator();

#ifndef NOT_PROTECTED
	deUint32					flags		= (protectionMode == PROTECTION_ENABLED)
												? vk::VK_IMAGE_CREATE_PROTECTED_BIT
												: (vk::VkImageCreateFlagBits)0u;
#else
	DE_UNREF(protectionMode);
	deUint32					flags		= 0u;
#endif

	const vk::VkImageCreateInfo	params		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			stype
		DE_NULL,										// const void*				pNext
		(vk::VkImageCreateFlags)flags,					// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,							// VkImageType				imageType
		format,											// VkFormat					format
		{ width, height, 1 },							// VkExtent3D				extent
		1u,												// deUint32					mipLevels
		1u,												// deUint32					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling
		usageFlags,										// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode
		1u,												// deUint32					queueFamilyIndexCount
		&queueFamilyIdx,								// const deUint32*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout
	};

#ifndef NOT_PROTECTED
	vk::MemoryRequirement		memReq		= (protectionMode == PROTECTION_ENABLED)
												? vk::MemoryRequirement::Protected
												: vk::MemoryRequirement::Any;
#else
	vk::MemoryRequirement		memReq		= vk::MemoryRequirement::Any;
#endif

	return de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, allocator, params, memReq));
}

de::MovePtr<vk::BufferWithMemory> makeBuffer (ProtectedContext&			context,
											  ProtectionMode			protectionMode,
											  const deUint32			queueFamilyIdx,
											  deUint32					size,
											  vk::VkBufferUsageFlags	usageFlags,
											  vk::MemoryRequirement		memReq)
{
	const vk::DeviceInterface&		vk			= context.getDeviceInterface();
	const vk::VkDevice&				device		= context.getDevice();
	vk::Allocator&					allocator	= context.getDefaultAllocator();

#ifndef NOT_PROTECTED
	deUint32						flags		= (protectionMode == PROTECTION_ENABLED)
													? vk::VK_BUFFER_CREATE_PROTECTED_BIT
													: (vk::VkBufferCreateFlagBits)0u;
	vk::MemoryRequirement			requirement	= memReq;
#else
	DE_UNREF(protectionMode);
	deUint32						flags		= 0u;
	vk::MemoryRequirement			requirement	= memReq & (
													vk::MemoryRequirement::HostVisible
													| vk::MemoryRequirement::Coherent
													| vk::MemoryRequirement::LazilyAllocated);
#endif

	const vk::VkBufferCreateInfo	params		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,									// pNext
		(vk::VkBufferCreateFlags)flags,				// flags
		(vk::VkDeviceSize)size,						// size
		usageFlags,									// usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,											// queueFamilyCount
		&queueFamilyIdx,							// pQueueFamilyIndices
	};

	return de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, params, requirement));
}

vk::Move<vk::VkImageView> createImageView (ProtectedContext& context, vk::VkImage image, vk::VkFormat format)
{
	const vk::VkImageViewCreateInfo params =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// sType
		DE_NULL,											// pNext
		0u,													// flags
		image,												// image
		vk::VK_IMAGE_VIEW_TYPE_2D,							// viewType
		format,												// format
		vk::makeComponentMappingRGBA(),						// components
		{ vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,1u },	// subresourceRange
	};

	return vk::createImageView(context.getDeviceInterface(), context.getDevice(), &params);
}

vk::Move<vk::VkRenderPass> createRenderPass (ProtectedContext& context, vk::VkFormat format)
{
	const vk::VkDevice					vkDevice				= context.getDevice();
	const vk::DeviceInterface&			vk						= context.getDeviceInterface();

	return vk::makeRenderPass(vk, vkDevice, format);
}

vk::Move<vk::VkFramebuffer> createFramebuffer (ProtectedContext& context, deUint32 width, deUint32 height,
												vk::VkRenderPass renderPass, vk::VkImageView colorImageView)
{
	const vk::VkDevice					vkDevice			= context.getDevice();
	const vk::DeviceInterface&			vk					= context.getDeviceInterface();

	const vk::VkFramebufferCreateInfo	framebufferParams	=
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkFramebufferCreateFlags	flags;
		renderPass,										// VkRenderPass				renderPass;
		1u,												// deUint32					attachmentCount;
		&colorImageView,								// const VkImageView*		pAttachments;
		width,											// deUint32					width;
		height,											// deUint32					height;
		1u												// deUint32					layers;
	};

	return vk::createFramebuffer(vk, vkDevice, &framebufferParams);
}


vk::Move<vk::VkPipelineLayout> createPipelineLayout (ProtectedContext& context, deUint32 layoutCount, vk::VkDescriptorSetLayout* setLayouts)
{
	const vk::VkPipelineLayoutCreateInfo	params	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
		DE_NULL,											// pNext
		0u,													// flags
		layoutCount,										// setLayoutCount
		setLayouts,											// pSetLayouts
		0u,													// pushConstantRangeCount
		DE_NULL,											// pPushContantRanges
	};

	return vk::createPipelineLayout(context.getDeviceInterface(), context.getDevice(), &params);
}

void beginSecondaryCommandBuffer (const vk::DeviceInterface&				vk,
								  const vk::VkCommandBuffer					secondaryCmdBuffer,
								  const vk::VkCommandBufferInheritanceInfo	bufferInheritanceInfo)
{
	const vk::VkCommandBufferUsageFlags	flags		= bufferInheritanceInfo.renderPass != DE_NULL
													  ? (vk::VkCommandBufferUsageFlags)vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
													  : (vk::VkCommandBufferUsageFlags)0u;
	const vk::VkCommandBufferBeginInfo	beginInfo	=
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// sType
		DE_NULL,													// pNext
		flags,														// flags
		&bufferInheritanceInfo,										// pInheritanceInfo
	};
	VK_CHECK(vk.beginCommandBuffer(secondaryCmdBuffer, &beginInfo));
}

vk::VkResult queueSubmit (ProtectedContext&		context,
						  ProtectionMode		protectionMode,
						  vk::VkQueue			queue,
						  vk::VkCommandBuffer	cmdBuffer,
						  vk::VkFence			fence,
						  deUint64				timeout)
{
	const vk::DeviceInterface&			vk			= context.getDeviceInterface();
	const vk::VkDevice&					device		= context.getDevice();

	// Basic submit info
	vk::VkSubmitInfo					submitInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
		DE_NULL,									// pNext
		0u,											// waitSemaphoreCount
		DE_NULL,									// pWaitSempahores
		(const vk::VkPipelineStageFlags*)DE_NULL,	// stageFlags
		1u,											// commandBufferCount
		&cmdBuffer,									// pCommandBuffers
		0u,											// signalSemaphoreCount
		DE_NULL,									// pSignalSemaphores
	};

#ifndef NOT_PROTECTED
	// Protected extension submit info
	const vk::VkProtectedSubmitInfo		protectedInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO,		// sType
		DE_NULL,											// pNext
		VK_TRUE,											// protectedSubmit
	};
	if (protectionMode == PROTECTION_ENABLED) {
		submitInfo.pNext = &protectedInfo;
	}
#else
	DE_UNREF(protectionMode);
#endif

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence));
	return vk.waitForFences(device, 1u, &fence, DE_TRUE, timeout);
}

vk::Move<vk::VkPipeline> makeComputePipeline (const vk::DeviceInterface&		vk,
											  const vk::VkDevice				device,
											  const vk::VkPipelineLayout		pipelineLayout,
											  const vk::VkShaderModule			shaderModule,
											  const vk::VkSpecializationInfo*	specInfo)
{
	const vk::VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		(vk::VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,												// VkShaderModule					module;
		"main",														// const char*						pName;
		specInfo,													// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const vk::VkComputePipelineCreateInfo pipelineInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(vk::VkPipelineCreateFlags)0,						// VkPipelineCreateFlags			flags;
		shaderStageInfo,									// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return vk::createComputePipeline(vk, device, DE_NULL , &pipelineInfo);
}

vk::Move<vk::VkSampler> makeSampler (const vk::DeviceInterface& vk, const vk::VkDevice& device)
{
	const vk::VkSamplerCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		DE_NULL,
		0u,

		vk::VK_FILTER_NEAREST,
		vk::VK_FILTER_NEAREST,

		vk::VK_SAMPLER_MIPMAP_MODE_LINEAR,
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f,
		VK_FALSE,
		1.0f,
		VK_FALSE,
		vk::VK_COMPARE_OP_ALWAYS,
		0.0f,
		0.0f,
		vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE
	};

	return vk::createSampler(vk, device, &createInfo);
}

vk::Move<vk::VkCommandPool> makeCommandPool (const vk::DeviceInterface&	vk,
											 const vk::VkDevice&		device,
											 ProtectionMode				protectionMode,
											 const deUint32				queueFamilyIdx)
{
	const deUint32	poolFlags	= vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
#ifndef NOT_PROTECTED
									| ((protectionMode == PROTECTION_ENABLED) ? vk::VK_COMMAND_POOL_CREATE_PROTECTED_BIT : 0x0)
#endif
									;
#ifdef NOT_PROTECTED
	DE_UNREF(protectionMode);
#endif

	return vk::createCommandPool(vk, device, poolFlags, queueFamilyIdx);
}

vk::Move<vk::VkPipeline> makeGraphicsPipeline (const vk::DeviceInterface&		vk,
											   const vk::VkDevice				device,
											   const vk::VkPipelineLayout		pipelineLayout,
											   const vk::VkRenderPass			renderPass,
											   const vk::VkShaderModule			vertexShaderModule,
											   const vk::VkShaderModule			fragmentShaderModule,
											   const VertexBindings&			vertexBindings,
											   const VertexAttribs&				vertexAttribs,
											   const tcu::UVec2&				renderSize,
											   const vk::VkPrimitiveTopology	topology)
{
	const std::vector<VkViewport>				viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>					scissors					(1, makeRect2D(renderSize));

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		(deUint32)vertexBindings.size(),							// deUint32                                    vertexBindingDescriptionCount;
		vertexBindings.data(),										// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		(deUint32)vertexAttribs.size(),								// deUint32                                    vertexAttributeDescriptionCount;
		vertexAttribs.data()										// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	return vk::makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
									device,								// const VkDevice                                device
									pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
									vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
									DE_NULL,							// const VkShaderModule                          tessellationControlModule
									DE_NULL,							// const VkShaderModule                          tessellationEvalModule
									DE_NULL,							// const VkShaderModule                          geometryShaderModule
									fragmentShaderModule,				// const VkShaderModule                          fragmentShaderModule
									renderPass,							// const VkRenderPass                            renderPass
									viewports,							// const std::vector<VkViewport>&                viewports
									scissors,							// const std::vector<VkRect2D>&                  scissors
									topology,							// const VkPrimitiveTopology                     topology
									0u,									// const deUint32                                subpass
									0u,									// const deUint32                                patchControlPoints
									&vertexInputStateCreateInfo);		// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

const char* getCmdBufferTypeStr (const CmdBufferType cmdBufferType)
{
	switch (cmdBufferType)
	{
		case CMD_BUFFER_PRIMARY:	return "primary";
		case CMD_BUFFER_SECONDARY:	return "secondary";

		default: DE_FATAL("Invalid command buffer type"); return "";
	}
}

void clearImage (ProtectedContext& ctx, vk::VkImage image)
{
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkClearColorValue			clearColor			= { { 0.0f, 0.0f, 0.0f, 0.0f } };

	const vk::VkImageSubresourceRange	subresourceRange	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,								// uint32_t				baseMipLevel
		1u,								// uint32_t				levelCount
		0u,								// uint32_t				baseArrayLayer
		1u,								// uint32_t				layerCount
	};

	const vk::VkImageMemoryBarrier		preImageBarrier		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
		vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange								// VkImageSubresourceRange	subresourceRange;
	};

	const vk::VkImageMemoryBarrier		postImageBarrier	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		vk::VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
		queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange								// VkImageSubresourceRange	subresourceRange;
	};

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  vk::VK_PIPELINE_STAGE_HOST_BIT,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  1, &preImageBarrier);
	vk.cmdClearColorImage(*cmdBuffer, image, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImageBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	{
		const vk::Unique<vk::VkFence>	fence		(createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}
}

void uploadImage (ProtectedContext& ctx, vk::VkImage image, const tcu::Texture2D& texture2D)
{
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_DISABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const deUint32						width				= (deUint32)texture2D.getWidth();
	const deUint32						height				= (deUint32)texture2D.getHeight();
	const deUint32						stagingBufferSize	= width * height * tcu::getPixelSize(texture2D.getFormat());

	de::UniquePtr<vk::BufferWithMemory>	stagingBuffer		(makeBuffer(ctx,
																		PROTECTION_DISABLED,
																		queueFamilyIndex,
																		stagingBufferSize,
																		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																		vk::MemoryRequirement::HostVisible));

	{
		const tcu::ConstPixelBufferAccess&	access		= texture2D.getLevel(0);
		const tcu::PixelBufferAccess		destAccess	(access.getFormat(), access.getSize(), stagingBuffer->getAllocation().getHostPtr());

		tcu::copy(destAccess, access);

		flushAlloc(vk, device, stagingBuffer->getAllocation());
	}

	const vk::VkImageSubresourceRange	subresourceRange	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,								// uint32_t				baseMipLevel
		1u,								// uint32_t				levelCount
		0u,								// uint32_t				baseArrayLayer
		1u,								// uint32_t				layerCount
	};

	const vk::VkImageMemoryBarrier		preCopyBarrier		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
		vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange								// VkImageSubresourceRange	subresourceRange;
	};

	const vk::VkImageMemoryBarrier		postCopyBarrier		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
		queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange								// VkImageSubresourceRange	subresourceRange;
	};

	const vk::VkImageSubresourceLayers	subresourceLayers	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,								// deUint32				mipLevel;
		0u,								// deUint32				baseArrayLayer;
		1u								// deUint32				layerCount;
	};

	const vk::VkBufferImageCopy			copyRegion			=
	{
		0u,								// VkDeviceSize					bufferOffset;
		width,							// deUint32						bufferRowLength;
		height,							// deUint32						bufferImageHeight;
		subresourceLayers,				// VkImageSubresourceLayers		imageSubresource;
		{ 0u, 0u, 0u },					// VkOffset3D					imageOffset;
		{ width, height, 1u }			// VkExtent3D					imageExtent;
	};

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_HOST_BIT,
						  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (vk::VkDependencyFlags)0u,
						  0u, (const vk::VkMemoryBarrier*)DE_NULL,
						  0u, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  1u, &preCopyBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, **stagingBuffer, image, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (vk::VkDependencyFlags)0u,
						  0u, (const vk::VkMemoryBarrier*)DE_NULL,
						  0u, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  1u, &postCopyBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	{
		const vk::Unique<vk::VkFence>	fence		(createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, PROTECTION_DISABLED, queue, *cmdBuffer, *fence, ~0ull));
	}
}

void copyToProtectedImage (ProtectedContext& ctx, vk::VkImage srcImage, vk::VkImage dstImage, vk::VkImageLayout dstImageLayout, deUint32 width, deUint32 height)
{
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkImageSubresourceRange	subresourceRange	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,								// uint32_t				baseMipLevel
		1u,								// uint32_t				levelCount
		0u,								// uint32_t				baseArrayLayer
		1u,								// uint32_t				layerCount
	};

	const vk::VkImageMemoryBarrier		preImageBarriers[]	=
	{
		// source image
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			vk::VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
			vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
			queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
			queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
			srcImage,										// VkImage					image;
			subresourceRange								// VkImageSubresourceRange	subresourceRange;
		},
		// destination image
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0,												// VkAccessFlags			srcAccessMask;
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
			vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
			queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
			queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
			dstImage,										// VkImage					image;
			subresourceRange								// VkImageSubresourceRange	subresourceRange;
		}
	};

	const vk::VkImageMemoryBarrier		postImgBarrier		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		vk::VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
		dstImageLayout,									// VkImageLayout			newLayout;
		queueFamilyIndex,								// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,								// deUint32					dstQueueFamilyIndex;
		dstImage,										// VkImage					image;
		subresourceRange								// VkImageSubresourceRange	subresourceRange;
	};

	const vk::VkImageSubresourceLayers	subresourceLayers	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,								// deUint32				mipLevel;
		0u,								// deUint32				baseArrayLayer;
		1u								// deUint32				layerCount;
	};

	const vk::VkImageCopy				copyImageRegion		=
	{
		subresourceLayers,		// VkImageSubresourceCopy	srcSubresource;
		{ 0, 0, 0 },			// VkOffset3D				srcOffset;
		subresourceLayers,		// VkImageSubresourceCopy	destSubresource;
		{ 0, 0, 0 },			// VkOffset3D				destOffset;
		{ width, height, 1u },	// VkExtent3D				extent;
	};

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  DE_LENGTH_OF_ARRAY(preImageBarriers), preImageBarriers);
	vk.cmdCopyImage(*cmdBuffer, srcImage, vk::VK_IMAGE_LAYOUT_GENERAL, dstImage, vk::VK_IMAGE_LAYOUT_GENERAL, 1u, &copyImageRegion);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
						  vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						  (vk::VkDependencyFlags)0,
						  0, (const vk::VkMemoryBarrier*)DE_NULL,
						  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImgBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	{
		const vk::Unique<vk::VkFence>	fence		(createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}
}

void fillWithRandomColorTiles (const tcu::PixelBufferAccess& dst, const tcu::Vec4& minVal, const tcu::Vec4& maxVal, deUint32 seed)
{
	const int	numCols		= dst.getWidth()  >= 7 ? 7 : dst.getWidth();
	const int	numRows		= dst.getHeight() >= 5 ? 5 : dst.getHeight();
	de::Random	rnd			(seed);

	for (int slice = 0; slice < dst.getDepth(); slice++)
	for (int row = 0; row < numRows; row++)
	for (int col = 0; col < numCols; col++)
	{
		const int	yBegin	= (row + 0)*dst.getHeight() / numRows;
		const int	yEnd	= (row + 1)*dst.getHeight() / numRows;
		const int	xBegin	= (col + 0)*dst.getWidth() / numCols;
		const int	xEnd	= (col + 1)*dst.getWidth() / numCols;
		tcu::Vec4	color;
		for (int i = 0; i < 4; i++)
			color[i] = rnd.getFloat(minVal[i], maxVal[i]);
		tcu::clear(tcu::getSubregion(dst, xBegin, yBegin, slice, xEnd - xBegin, yEnd - yBegin, 1), color);
	}
}

void fillWithUniqueColors (const tcu::PixelBufferAccess& dst, deUint32 seed)
{
	// This is an implementation of linear congruential generator.
	// The A and M are prime numbers, thus allowing to generate unique number sequence of length genM-1.
	// The generator uses C constant as 0, thus value of 0 is not allowed as a seed.
	const deUint64	genA	= 1573051ull;
	const deUint64	genM	= 2097023ull;
	deUint64		genX	= seed % genM;

	DE_ASSERT(deUint64(dst.getWidth()) * deUint64(dst.getHeight()) * deUint64(dst.getDepth()) < genM - 1);

	if (genX == 0)
		genX = 1;

	const int	numCols		= dst.getWidth();
	const int	numRows		= dst.getHeight();
	const int	numSlices	= dst.getDepth();

	for (int z = 0; z < numSlices; z++)
	for (int y = 0; y < numRows; y++)
	for (int x = 0; x < numCols; x++)
	{
		genX = (genA * genX) % genM;

		DE_ASSERT(genX != seed);

		const float		r		= float(deUint32((genX >> 0)  & 0x7F)) / 127.0f;
		const float		g		= float(deUint32((genX >> 7)  & 0x7F)) / 127.0f;
		const float		b		= float(deUint32((genX >> 14) & 0x7F)) / 127.0f;
		const tcu::Vec4	color	= tcu::Vec4(r, g, b, 1.0f);

		dst.setPixel(color, x, y, z);
	}
}

} // ProtectedMem
} // vkt
