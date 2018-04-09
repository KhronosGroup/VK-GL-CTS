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

#include "deString.h"

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

std::vector<std::string> getValidationLayers (const vk::PlatformInterface& vkp)
{
	static const char*	s_magicLayer		= "VK_LAYER_LUNARG_standard_validation";
	static const char*	s_defaultLayers[]	=
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"
	};
	const std::vector<vk::VkLayerProperties>	supportedLayers	(enumerateInstanceLayerProperties(vkp));
	std::vector<std::string>					enabledLayers;

	if (isLayerSupported(supportedLayers, vk::RequiredLayer(s_magicLayer)))
		enabledLayers.push_back(s_magicLayer);
	else
	{
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_defaultLayers); ++ndx)
		{
			if (isLayerSupported(supportedLayers, vk::RequiredLayer(s_defaultLayers[ndx])))
				enabledLayers.push_back(s_defaultLayers[ndx]);
		}
	}

	return enabledLayers;
}


vk::Move<vk::VkInstance> makeProtectedMemInstance (const vk::PlatformInterface& vkp, const vkt::Context& context, const std::vector<std::string>& extraExtensions)
{
	const Extensions			supportedExtensions(vk::enumerateInstanceExtensionProperties(vkp, DE_NULL));
	std::vector<std::string>	enabledLayers;
	std::vector<std::string>	requiredExtensions = extraExtensions;
	const bool					isValidationEnabled	= context.getTestContext().getCommandLine().isValidationEnabled();

	if (isValidationEnabled)
	{
		if (!vk::isDebugReportSupported(vkp))
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		enabledLayers = getValidationLayers(vkp);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	if (!isCoreInstanceExtension(context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"))
		requiredExtensions.push_back("VK_KHR_get_physical_device_properties2");

	for (std::vector<std::string>::const_iterator requiredExtName = requiredExtensions.begin();
		requiredExtName != requiredExtensions.end();
		++requiredExtName)
	{
		if (!isInstanceExtensionSupported(context.getUsedApiVersion(), supportedExtensions, vk::RequiredExtension(*requiredExtName)))
			TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
	}

	return vk::createDefaultInstance(vkp, context.getUsedApiVersion(), enabledLayers, requiredExtensions);
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
												 const std::vector<std::string>&	extraExtensions)
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
		if (!isDeviceExtensionSupported(apiVersion, supportedExtensions, vk::RequiredExtension(extensions[ndx])))
			TCU_THROW(NotSupportedError, (extensions[ndx] + " is not supported").c_str());

		if (!isCoreDeviceExtension(apiVersion, extensions[ndx]))
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

	return vk::createDevice(vkp, instance, vkd, physicalDevice, &deviceParams, DE_NULL);
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

	// Protected extension submit info
	const vk::VkProtectedSubmitInfo		protectedInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO,		// sType
		DE_NULL,											// pNext
		VK_TRUE,											// protectedSubmit
	};
#ifndef NOT_PROTECTED
	if (protectionMode == PROTECTION_ENABLED) {
		submitInfo.pNext = &protectedInfo;
	}
#endif

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence));
	return vk.waitForFences(device, 1u, &fence, DE_TRUE, timeout);
}

vk::Move<vk::VkDescriptorSet> makeDescriptorSet (const vk::DeviceInterface&			vk,
												 const vk::VkDevice					device,
												 const vk::VkDescriptorPool			descriptorPool,
												 const vk::VkDescriptorSetLayout	setLayout)
{
	const vk::VkDescriptorSetAllocateInfo allocateParams =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		descriptorPool,										// VkDescriptorPool				descriptorPool;
		1u,													// deUint32						setLayoutCount;
		&setLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
	};
	return vk::allocateDescriptorSet(vk, device, &allocateParams);
}


vk::Move<vk::VkPipelineLayout> makePipelineLayout (const vk::DeviceInterface&		vk,
												   const vk::VkDevice				device,
												   const vk::VkDescriptorSetLayout	descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo info =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		(vk::VkPipelineLayoutCreateFlags)0,					// VkPipelineLayoutCreateFlags	flags;
		1u,													// deUint32						setLayoutCount;
		&descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
		0u,													// deUint32						pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*	pPushConstantRanges;
	};
	return vk::createPipelineLayout(vk, device, &info);
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

} // ProtectedMem
} // vkt
