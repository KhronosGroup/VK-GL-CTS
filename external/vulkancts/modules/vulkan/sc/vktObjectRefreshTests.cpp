/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan SC VK_KHR_object_refresh Tests
*//*--------------------------------------------------------------------*/

#include "vktObjectRefreshTests.hpp"

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"

#include <map>
#include <vector>

namespace vkt
{
namespace sc
{
namespace
{

tcu::TestStatus queryRefreshableObjects(Context& context)
{
	deUint32						countReported	= 0u;
	vk::VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const vk::InstanceInterface&	vki				= context.getInstanceInterface();

	// get number of refreshable objects
	vk::VkResult result = vki.getPhysicalDeviceRefreshableObjectTypesKHR(physicalDevice, &countReported, DE_NULL);
	if (result != vk::VK_SUCCESS)
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR returned invalid return code");

	if (countReported == 0)
		TCU_THROW(NotSupportedError, "No refreshable objects available");

	deUint32						refreshableObjectsMaxCount	(countReported + 2);
	std::vector<vk::VkObjectType>	refreshableObjects			(refreshableObjectsMaxCount);

	for (deUint32 countRequested = 0; countRequested < refreshableObjectsMaxCount; ++countRequested)
	{
		// get refreshable objects
		deUint32 countRetrieved = countRequested;
		std::fill(refreshableObjects.begin(), refreshableObjects.end(), vk::VK_OBJECT_TYPE_UNKNOWN);
		result = vki.getPhysicalDeviceRefreshableObjectTypesKHR(physicalDevice, &countRetrieved, refreshableObjects.data());

		// verify returned code
		if ((result != vk::VK_SUCCESS) && (result != vk::VK_INCOMPLETE))
			TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR returned invalid return code");

		// verify number of retrieved objects
		if (countRetrieved != std::min(countRequested, countReported))
			TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR returned invalid number of retrieved objects");

		// verify retrieved objects
		for (deUint32 i = 0; i < countRetrieved; ++i)
		{
			if (refreshableObjects[i] == vk::VK_OBJECT_TYPE_UNKNOWN)
				TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR returned invalid object type");
		}
	}

	return tcu::TestStatus::pass("pass");
}

tcu::TestStatus refreshObjects(Context& context, bool individualRefresh)
{
	deUint32						countReported	= 0u;
	vk::VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const vk::InstanceInterface&	vki				= context.getInstanceInterface();
	const vk::DeviceInterface&		vkd				= context.getDeviceInterface();

	vk::VkResult result = vki.getPhysicalDeviceRefreshableObjectTypesKHR(physicalDevice, &countReported, DE_NULL);
	if ((result != vk::VK_SUCCESS) || (countReported == 0))
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR failed");

	std::vector<vk::VkObjectType> refreshableObjectTypes(countReported);
	result = vki.getPhysicalDeviceRefreshableObjectTypesKHR(physicalDevice, &countReported, refreshableObjectTypes.data());
	if (result != vk::VK_SUCCESS)
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceRefreshableObjectTypesKHR failed");

	// create all possible objects
	const deUint32						queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	vk::VkDevice						device				= context.getDevice();
	vk::VkQueue							queue				= context.getUniversalQueue();
	vk::Allocator&						allocator			= context.getDefaultAllocator();
	vk::Move<vk::VkCommandPool>			cmdPool				= createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	vk::Move<vk::VkCommandBuffer>		cmdBuffer			= allocateCommandBuffer(vkd, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	vk::Move<vk::VkFence>				fence				= createFence(vkd, device);
	vk::Move<vk::VkSemaphore>			semaphore			= createSemaphore(vkd, device);
	vk::Move<vk::VkEvent>				event				= createEvent(vkd, device);
	const vk::VkQueryPoolCreateInfo		queryPoolCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,						// VkStructureType						sType;
		DE_NULL,															// const void*							pNext;
		0u,																	// VkQueryPoolCreateFlags				flags;
		vk::VK_QUERY_TYPE_OCCLUSION,										// VkQueryType							queryType;
		1u,																	// deUint32								queryCount;
		0u,																	// VkQueryPipelineStatisticFlags		pipelineStatistics;
	};
	vk::Move<vk::VkQueryPool>			queryPool			= createQueryPool(vkd, device, &queryPoolCreateInfo);
	const vk::VkBufferCreateInfo		bufferCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,												// VkStructureType						sType
		DE_NULL,																				// const void*							pNext
		0u,																						// VkBufferCreateFlags					flags
		(vk::VkDeviceSize)64,																	// VkDeviceSize							size
		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,	// VkBufferUsageFlags					usage
		vk::VK_SHARING_MODE_EXCLUSIVE,															// VkSharingMode						sharingMode
		0u,																						// deUint32								queueFamilyIndexCount
		DE_NULL																					// const deUint32*						pQueueFamilyIndices
	};
	vk::Move<vk::VkBuffer>				buffer				= createBuffer(vkd, device, &bufferCreateInfo);
	const vk::VkMemoryRequirements		bufferRequirements	= getBufferMemoryRequirements(vkd, device, *buffer);
	de::MovePtr<vk::Allocation>			bufferAllocation	= allocator.allocate(bufferRequirements, vk::MemoryRequirement::HostVisible);
	vkd.bindBufferMemory(device, *buffer, bufferAllocation->getMemory(), 0);
	vk::Move<vk::VkBufferView>			bufferView			= makeBufferView(vkd, device, *buffer, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0ull, VK_WHOLE_SIZE);
	const vk::VkSamplerCreateInfo		samplerCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,							// VkStructureType						sType;
		DE_NULL,															// const void*							pNext;
		(vk::VkSamplerCreateFlags)0,										// VkSamplerCreateFlags					flags;
		vk::VK_FILTER_NEAREST,												// VkFilter								magFilter;
		vk::VK_FILTER_NEAREST,												// VkFilter								minFilter;
		vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,									// VkSamplerMipmapMode					mipmapMode;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							// VkSamplerAddressMode					addressModeU;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							// VkSamplerAddressMode					addressModeV;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							// VkSamplerAddressMode					addressModeW;
		0.0f,																// float								mipLodBias;
		VK_FALSE,															// VkBool32								anisotropyEnable;
		1.0f,																// float								maxAnisotropy;
		VK_FALSE,															// VkBool32								compareEnable;
		vk::VK_COMPARE_OP_ALWAYS,											// VkCompareOp							compareOp;
		0.0f,																// float								minLod;
		0.0f,																// float								maxLod;
		vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,						// VkBorderColor						borderColor;
		VK_FALSE,															// VkBool32								unnormalizedCoordinates;
	};
	vk::Move<vk::VkSampler>							sampler			= createSampler(vkd, device, &samplerCreateInfo);
	const vk::VkSamplerYcbcrConversionCreateInfo	conversionInfo
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,															// const void*							pNext;
		vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,							// VkFormat								format;
		vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,					// VkSamplerYcbcrModelConversion		ycbcrModel;
		vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL,								// VkSamplerYcbcrRange					ycbcrRange;
		{																	// VkComponentMapping					components;
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY,
			vk::VK_COMPONENT_SWIZZLE_IDENTITY
		},
		vk::VK_CHROMA_LOCATION_MIDPOINT,									// VkChromaLocation						xChromaOffset;
		vk::VK_CHROMA_LOCATION_MIDPOINT,									// VkChromaLocation						yChromaOffset;
		vk::VK_FILTER_NEAREST,												// VkFilter								chromaFilter;
		DE_FALSE															// VkBool32								forceExplicitReconstruction;
	};
	vk::Move<vk::VkSamplerYcbcrConversion>			ycbcrConversion = vk::createSamplerYcbcrConversion(vkd, device, &conversionInfo);
	const vk::VkImageCreateInfo						imageCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											// VkStructureType						sType;
		DE_NULL,																			// const void*							pNext;
		0u,																					// VkImageCreateFlags					flags;
		vk::VK_IMAGE_TYPE_2D,																// VkImageType							imageType;
		vk::VK_FORMAT_R8G8B8A8_UNORM,														// VkFormat								format;
		{ 64, 64, 1 },																		// VkExtent3D							extent;
		1u,																					// deUint32								mipLevels;
		1u,																					// deUint32								arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,															// VkSampleCountFlagBits				samples;
		vk::VK_IMAGE_TILING_OPTIMAL,														// VkImageTiling						tiling;
		vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,		// VkImageUsageFlags					usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,														// VkSharingMode						sharingMode;
		1u,																					// deUint32								queueFamilyIndexCount;
		&queueFamilyIndex,																	// const deUint32*						pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout						initialLayout;
	};
	vk::Move<vk::VkImage>					image				= createImage(vkd, device, &imageCreateInfo);
	const vk::VkMemoryRequirements			imageRequirements	= getImageMemoryRequirements(vkd, device, *image);
	de::MovePtr<vk::Allocation>				imageAllocation		= allocator.allocate(imageRequirements, vk::MemoryRequirement::Any);
	vkd.bindImageMemory(device, *image, imageAllocation->getMemory(), imageAllocation->getOffset());
	vk::Move<vk::VkImageView>				imageView			= makeImageView(vkd, device, *image, vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R8G8B8A8_UNORM, makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	vk::Move<vk::VkShaderModule>			shaderModule		= createShaderModule(vkd, device, context.getBinaryCollection().get("comp"), 0u);
	vk::Move<vk::VkRenderPass>				renderPass			= makeRenderPass(vkd, device, vk::VK_FORMAT_R8G8B8A8_UNORM);
	vk::Move<vk::VkFramebuffer>				framebuffer			= makeFramebuffer(vkd, device, *renderPass, *imageView, 64, 64);
	const vk::VkPipelineCacheCreateInfo		pipelineCacheCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,					// VkStructureType						sType;
		DE_NULL,															// const void*							pNext;
		vk::VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
			vk::VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,		// VkPipelineCacheCreateFlags	flags;
		context.getResourceInterface()->getCacheDataSize(),					// deUintptr					initialDataSize;
		context.getResourceInterface()->getCacheData()						// const void*					pInitialData;
	};
	vk::Move<vk::VkPipelineCache>			pipelineCache		= createPipelineCache(vkd, device, &pipelineCacheCreateInfo);
	vk::Move<vk::VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vkd, device);
	vk::Move<vk::VkPipeline>				pipeline			= makeComputePipeline(vkd, device, *pipelineLayout, *shaderModule);
	const vk::VkDescriptorPoolSize			descriptorPoolSize
	{
		vk::VK_DESCRIPTOR_TYPE_SAMPLER,										// VkDescriptorType						type;
		8																	// deUint32								descriptorCount;
	};
	const vk::VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,					// VkStructureType						sType;
		NULL,																// const void*							pNext;
		0u,																	// VkDescriptorPoolCreateFlags			flags;
		8,																	// deUint32								maxSets;
		1,																	// deUint32								poolSizeCount;
		&descriptorPoolSize													// const VkDescriptorPoolSize*			pPoolSizes;
	};
	vk::Move<vk::VkDescriptorPool>			descriptorPool = createDescriptorPool(vkd, device, &descriptorPoolCreateInfo);
	const vk::VkDescriptorSetLayoutBinding	descriptorSetLayoutBinding
	{
		0,																	// deUint32								binding;
		vk::VK_DESCRIPTOR_TYPE_SAMPLER,										// VkDescriptorType						descriptorType;
		1,																	// deUint32								descriptorCount;
		vk::VK_SHADER_STAGE_ALL,											// VkShaderStageFlags					stageFlags;
		NULL																// const VkSampler*						pImmutableSamplers;
	};
	const vk::VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,			// VkStructureType						sType;
		NULL,																// const void*							pNext;
		0,																	// VkDescriptorSetLayoutCreateFlags		flags;
		1,																	// deUint32								bindingCount;
		&descriptorSetLayoutBinding											// const VkDescriptorSetLayoutBinding*	pBindings;
	};
	vk::Move<vk::VkDescriptorSetLayout>		descriptorSetLayout	= createDescriptorSetLayout(vkd, device, &descriptorSetLayoutInfo);
	vk::Move<vk::VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

	std::map<vk::VkObjectType, deUint64>	objectHandlesMap
	{
		{ vk::VK_OBJECT_TYPE_INSTANCE,						0 },
		{ vk::VK_OBJECT_TYPE_PHYSICAL_DEVICE,				0 },
		{ vk::VK_OBJECT_TYPE_DEVICE,						0 },
		{ vk::VK_OBJECT_TYPE_QUEUE,							0 },
		{ vk::VK_OBJECT_TYPE_SEMAPHORE,						semaphore.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_COMMAND_BUFFER,				0 },
		{ vk::VK_OBJECT_TYPE_FENCE,							fence.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_DEVICE_MEMORY,					bufferAllocation->getMemory().getInternal() },
		{ vk::VK_OBJECT_TYPE_BUFFER,						buffer.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_IMAGE,							image.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_EVENT,							event.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_QUERY_POOL,					queryPool.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_BUFFER_VIEW,					bufferView.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_IMAGE_VIEW,					imageView.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_SHADER_MODULE,					shaderModule.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_PIPELINE_CACHE,				pipelineCache.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_PIPELINE_LAYOUT,				pipelineLayout.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_RENDER_PASS,					renderPass.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_PIPELINE,						pipeline.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,			descriptorSetLayout.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_SAMPLER,						sampler.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_DESCRIPTOR_POOL,				descriptorPool.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_DESCRIPTOR_SET,				descriptorSet.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_FRAMEBUFFER,					framebuffer.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_COMMAND_POOL,					cmdPool.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION,		ycbcrConversion.get().getInternal() },
		{ vk::VK_OBJECT_TYPE_SURFACE_KHR,					0 },
		{ vk::VK_OBJECT_TYPE_SWAPCHAIN_KHR,					0 },
		{ vk::VK_OBJECT_TYPE_DISPLAY_KHR,					0 },
		{ vk::VK_OBJECT_TYPE_DISPLAY_MODE_KHR,				0 },
		{ vk::VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,		0 }
	};

	const vk::VkMemoryBarrier objRefreshBarrier
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_MEMORY_READ_BIT
	};

	// record command buffer
	vk::beginCommandBuffer(vkd, *cmdBuffer);

	if (individualRefresh)
	{
		for (const auto& object : objectHandlesMap)
		{
			vk::VkObjectType	objectType		= object.first;
			deUint64			objectHandle	= object.second;

			// skip object type if it can't be refreshed on current implementation
			if (std::find(refreshableObjectTypes.begin(), refreshableObjectTypes.end(), objectType) == refreshableObjectTypes.end())
				continue;

			if (!objectHandle)
				continue;

			vk::VkRefreshObjectKHR		refreshObject = { objectType, objectHandle, 0 };
			vk::VkRefreshObjectListKHR	refreshList = { vk::VK_STRUCTURE_TYPE_REFRESH_OBJECT_LIST_KHR, DE_NULL, 1, &refreshObject };
			vkd.cmdRefreshObjectsKHR(*cmdBuffer, &refreshList);
			vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &objRefreshBarrier, 0, DE_NULL, 0, DE_NULL);
		}
	}
	else
	{
		deUint32 countUsed = 0;
		std::vector<vk::VkRefreshObjectKHR> objectsToRefreshList(countReported);
		for (deUint32 i = 0 ; i < countReported ; ++i)
		{
			vk::VkObjectType	objectType		= refreshableObjectTypes[i];
			deUint64			objectHandle	= objectHandlesMap.at(objectType);

			if (!objectHandle)
				continue;

			objectsToRefreshList[countUsed++] =
			{
				objectType,
				objectHandle,
				0
			};
		}

		vk::VkRefreshObjectListKHR refreshList = { vk::VK_STRUCTURE_TYPE_REFRESH_OBJECT_LIST_KHR, DE_NULL, countUsed, objectsToRefreshList.data() };
		vkd.cmdRefreshObjectsKHR(*cmdBuffer, &refreshList);
		vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &objRefreshBarrier, 0, DE_NULL, 0, DE_NULL);
	}

	vk::endCommandBuffer(vkd, *cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	return tcu::TestStatus::pass("Pass");
}

void createComputeSource(vk::SourceCollections& dst)
{
	dst.glslSources.add("comp") << glu::ComputeSource(
		"#version 450\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"void main (void)\n"
		"{\n"
		"    vec4 dummy = vec4(1.0);\n"
		"}\n");
}

tcu::TestStatus refreshIndividualObjects(Context& context)
{
	return refreshObjects(context, true);
}

tcu::TestStatus refreshAllObjects(Context& context)
{
	return refreshObjects(context, false);
}

void checkRefreshSupport(Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_object_refresh");
}

} // anonymous

tcu::TestCaseGroup* createObjectRefreshTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "object_refresh", "Tests VK_KHR_object_refresh"));

	addFunctionCase(group.get(),				"query_refreshable_objects",	"Test VK_KHR_object_refresh extension", checkRefreshSupport, queryRefreshableObjects);
	addFunctionCaseWithPrograms(group.get(),	"refresh_individual_objects",	"Test VK_KHR_object_refresh extension", checkRefreshSupport, createComputeSource, refreshIndividualObjects);
	addFunctionCaseWithPrograms(group.get(),	"refresh_all_objects",			"Test VK_KHR_object_refresh extension", checkRefreshSupport, createComputeSource, refreshAllObjects);

	return group.release();
}

}	// sc

}	// vkt
