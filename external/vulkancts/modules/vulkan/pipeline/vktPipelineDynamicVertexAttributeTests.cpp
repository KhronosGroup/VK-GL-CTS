/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google Inc.
 * Copyright (c) 2023 LunarG, Inc.
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
 * \brief Dynamic vertex attribute tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDynamicVertexAttributeTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineExtendedDynamicStateTests.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include <array>
#include <set>

namespace vkt
{
namespace pipeline
{

namespace
{

vk::VkVertexInputAttributeDescription2EXT makeVertexInputAttributeDescription2EXT (deUint32 location, deUint32 binding, vk::VkFormat format, deUint32 offset)
{
	vk::VkVertexInputAttributeDescription2EXT desc = vk::initVulkanStructure();

	desc.location	= location;
	desc.binding	= binding;
	desc.format		= format;
	desc.offset		= offset;

	return desc;
}

vk::VkVertexInputBindingDescription2EXT makeVertexInputBindingDescription2EXT (deUint32 binding, deUint32 stride, vk::VkVertexInputRate inputRate)
{
	vk::VkVertexInputBindingDescription2EXT desc = vk::initVulkanStructure();

	desc.binding	= binding;
	desc.stride		= stride;
	desc.inputRate	= inputRate;
	desc.divisor	= 1u;

	return desc;
}

vk::VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const vk::VkFormat format, const vk::VkImageUsageFlags usage)
{
	const vk::VkImageCreateInfo imageParams =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(vk::VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,										// VkFormat					format;
		vk::makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,											// deUint32					queueFamilyIndexCount;
		DE_NULL,									// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	return imageParams;
}

static std::vector <std::string> removeExtensions (const std::vector<std::string>& a, const std::vector<const char*>& b)
{
	std::vector<std::string>	res;
	std::set<std::string>		removeExts(b.begin(), b.end());

	for (std::vector<std::string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
	{
		if (!de::contains(removeExts, *aIter))
			res.push_back(*aIter);
	}

	return res;
}

vk::Move<vk::VkDevice> createDynamicVertexStateDevice (Context& context, const deUint32 testQueueFamilyIndex, const vk::PipelineConstructionType pipelineConstructionType)
{
	DE_UNREF(pipelineConstructionType);

	void* pNext = DE_NULL;

#ifndef CTS_USES_VULKANSC
	vk::VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT	graphicsPipelineFeatures
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,	// VkStructureType	sType;
		pNext,																			// void*			pNext;
		VK_TRUE,																		// VkBool32			graphicsPipelineLibrary;
	};

	if (vk::isConstructionTypeLibrary(pipelineConstructionType))
		pNext = &graphicsPipelineFeatures;
	vk::VkPhysicalDeviceDynamicRenderingFeaturesKHR			dynamicRenderingFeatures
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,	// VkStructureType	sType;
		pNext,																	// void* pNext;
		VK_TRUE,																// VkBool32		dynamicRendering;
	};
	vk::VkPhysicalDeviceShaderObjectFeaturesEXT				shaderObjectFeatures
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,	// VkStructureType	sType;
		&dynamicRenderingFeatures,											// void*			pNext;
		VK_TRUE,															// VkBool32			shaderObject;
	};

	if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
		pNext = &shaderObjectFeatures;
#endif

	vk::VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT	dynamicVertexState
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,	// VkStructureType	sType;
		pNext,																			// void*			pNext;
		VK_TRUE,																		// VkBool32			vertexInputDynamicState;
	};

	vk::VkPhysicalDeviceFeatures2							physDeviceFeats2	= context.getDeviceFeatures2();

	physDeviceFeats2.features	= context.getDeviceFeatures();
	physDeviceFeats2.pNext		= &dynamicVertexState;

	const float												queuePriority		= 1.0f;

	const vk::VkDeviceQueueCreateInfo						queueParams			=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkDeviceQueueCreateFlags	flags;
		testQueueFamilyIndex,							// deUint32					queueFamilyIndex;
		1u,												// deUint32					queueCount;
		&queuePriority									// const float*				pQueuePriorities;
	};

	std::vector<const char*>							extensionPtrs;
	std::vector<const char*>							coreExtensions;

	vk::getCoreDeviceExtensions(context.getUsedApiVersion(), coreExtensions);

	std::vector<std::string>							nonCoreExtensions	(removeExtensions(context.getDeviceExtensions(), coreExtensions));

	extensionPtrs.resize(nonCoreExtensions.size());

	for (size_t ndx = 0; ndx < nonCoreExtensions.size(); ++ndx)
		extensionPtrs[ndx] = nonCoreExtensions[ndx].c_str();

	const vk::VkDeviceCreateInfo						deviceInfo			=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	// VkStructureType					sType;
		&physDeviceFeats2,							// const void*						pNext;
		0u,											// VkDeviceCreateFlags				flags;
		1u,											// deUint32							queueCreateInfoCount;
		&queueParams,								// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,											// deUint32							enabledLayerCount;
		DE_NULL,									// const char* const*				ppEnabledLayerNames;
		(deUint32)extensionPtrs.size(),				// deUint32							enabledExtensionCount;
		extensionPtrs.data(),						// const char* const*				ppEnabledExtensionNames;
		NULL										// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(),
		context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceInfo);
}

class NonSequentialInstance : public TestInstance
{
	public:
								NonSequentialInstance		(Context&							context,
															 const vk::PipelineConstructionType	pipelineConstructionType,
															 const deUint32						numInstances,
															 const std::vector<deUint32>		attributeLocations)
								: TestInstance					(context)
								, m_pipelineConstructionType	(pipelineConstructionType)
								, m_numInstances				(numInstances)
								, m_attributeLocations			(attributeLocations)
								{}

								~NonSequentialInstance	()
								{}

	virtual tcu::TestStatus		iterate						(void);

private:
	struct VertexInfo
	{
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const deUint32						m_numInstances;
	const std::vector<deUint32>			m_attributeLocations;
};

tcu::TestStatus NonSequentialInstance::iterate (void)
{
	tcu::TestLog&													log						= m_context.getTestContext().getLog();
	const vk::DeviceInterface&										vk						= m_context.getDeviceInterface();
	const vk::PlatformInterface&									vkp						= m_context.getPlatformInterface();
	const vk::VkInstance											vki						= m_context.getInstance();
	const vk::InstanceInterface&									instanceInterface		= m_context.getInstanceInterface();
	const deUint32													queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const vk::VkPhysicalDevice										physicalDevice			= m_context.getPhysicalDevice();
	const vk::Move<vk::VkDevice>									device					= createDynamicVertexStateDevice(m_context, queueFamilyIndex, m_pipelineConstructionType);
	vk::SimpleAllocator												allocator				(vk, *device, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const vk::DeviceDriver											deviceDriver			(vkp, vki, *device, m_context.getUsedApiVersion());
	const vk::VkQueue												queue					= getDeviceQueue(deviceDriver, *device, queueFamilyIndex, 0u);
	const auto&														deviceExtensions		= m_context.getDeviceExtensions();

	// Create shaders
	const std::array<vk::ShaderWrapper, 2>							vertexShaderModules		=
	{
		vk::ShaderWrapper(vk, *device, m_context.getBinaryCollection().get("vert_0")),
		vk::ShaderWrapper(vk, *device, m_context.getBinaryCollection().get("vert_1"))
	};

	const vk::ShaderWrapper											fragmentShaderModule	= vk::ShaderWrapper(vk, *device, m_context.getBinaryCollection().get("frag"));

	const deUint32													vertexBufferBindIndex	= 0u;

	// Vertex input state and binding
	const vk::VkVertexInputBindingDescription2EXT					bindingDescription2EXT	= makeVertexInputBindingDescription2EXT(vertexBufferBindIndex, sizeof(VertexInfo), vk::VK_VERTEX_INPUT_RATE_VERTEX);

	const std::array<vk::VkVertexInputAttributeDescription2EXT, 2>		vertexInputAttributeDesc2EXTGreens
	{
		makeVertexInputAttributeDescription2EXT(0, vertexBufferBindIndex, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
		makeVertexInputAttributeDescription2EXT(m_attributeLocations[0], vertexBufferBindIndex, vk::VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float)) * 4u)
	};

	const std::array<vk::VkVertexInputAttributeDescription2EXT, 2>		vertexInputAttributeDesc2EXT2Reds
	{
		makeVertexInputAttributeDescription2EXT(0, vertexBufferBindIndex, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
		makeVertexInputAttributeDescription2EXT(m_attributeLocations[1], vertexBufferBindIndex, vk::VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float)) * 4u)
	};

	const vk::VkPipelineVertexInputStateCreateInfo						vertexInputStateCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(vk::VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags;
		0u,																// uint32_t									vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,																// uint32_t									vertexAttributeDescriptionCount;
		DE_NULL															// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	vk::Move<vk::VkImage>											colorImage				= (makeImage(vk, *device, makeImageCreateInfo(tcu::IVec2(32, 32), vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));

	// Allocate and bind color image memory
	const vk::VkImageSubresourceRange								colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::UniquePtr<vk::Allocation>								colorImageAlloc			(bindImage(vk,*device, allocator, *colorImage, vk::MemoryRequirement::Any));
	vk::Move<vk::VkImageView>										colorImageView			= (makeImageView(vk,*device, *colorImage, vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R8G8B8A8_UNORM, colorSubresourceRange));

	// Create renderpass
	const vk::VkAttachmentDescription								attachmentDescription	=
	{
		(vk::VkAttachmentDescriptionFlags)0u,		// VkAttachmentDescriptionFlags	flags
		vk::VK_FORMAT_R8G8B8A8_UNORM,				// VkFormat						format
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits		samples
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,			// VkAttachmentLoadOp			loadOp
		vk::VK_ATTACHMENT_STORE_OP_STORE,			// VkAttachmentStoreOp			storeOp
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// VkAttachmentLoadOp			stencilLoadOp
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,		// VkAttachmentStoreOp			stencilStoreOp
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout				initialLayout
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL// VkImageLayout				finalLayout
	};

	const vk::VkAttachmentReference									attachmentReference		=
	{
		0u,												// deUint32			attachment
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
	};

	const vk::VkSubpassDescription									subpassDescription		=
	{
		(vk::VkSubpassDescriptionFlags)0u,	// VkSubpassDescriptionFlags	flags
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,// VkPipelineBindPoint			pipelineBindPoint
		0u,									// deUint32						inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*	pInputAttachments
		1u,									// deUint32						colorAttachmentCount
		&attachmentReference,				// const VkAttachmentReference*	pColorAttachments
		DE_NULL,							// const VkAttachmentReference*	pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*	pDepthStencilAttachment
		0u,									// deUint32						preserveAttachmentCount
		DE_NULL								// const deUint32*				pPreserveAttachments
	};

	const vk::VkRenderPassCreateInfo								renderPassInfo			=
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureTypei					sType
		DE_NULL,										// const void*						pNext
		(vk::VkRenderPassCreateFlags)0u,				// VkRenderPassCreateFlags			flags
		1u,												// deUint32							attachmentCount
		&attachmentDescription,							// const VkAttachmentDescription*	pAttachments
		1u,												// deUint32							subpassCount
		&subpassDescription,							// const VkSubpassDescription*		pSubpasses
		0u,												// deUint32							dependencyCount
		DE_NULL											// const VkSubpassDependency*		pDependencies
	};

	vk::RenderPassWrapper											renderPass				= vk::RenderPassWrapper(m_pipelineConstructionType, vk, *device, &renderPassInfo);

	// Create framebuffer
	const vk::VkImageView											attachmentBindInfos[]	=
	{
		*colorImageView
	};

	const vk::VkFramebufferCreateInfo								framebufferCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		vk::VkFramebufferCreateFlags(0),				// VkFramebufferCreateFlags		flags;
		*renderPass,									// VkRenderPass					renderPass;
		1u,												// deUint32						attachmentCount;
		attachmentBindInfos,							// const VkImageView*			pAttachments;
		32u,											// deUint32						width;
		32u,											// deUint32						height;
		1u												// deUint32						layers;
	};

	renderPass.createFramebuffer(vk, *device, &framebufferCreateInfo, { *colorImage });

	std::array<vk::VkDynamicState, 1>								dynamicStates
	{
		vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
	};

	vk::VkPipelineDynamicStateCreateInfo							pipelineDynamicStateNfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,													// const void*							pNext;
		(vk::VkPipelineDynamicStateCreateFlags)0u,					// VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				// uint32_t								dynamicStateCount;
		dynamicStates.data()										// const VkDynamicState*				pDynamicStates;
	};

	// Create pipeline layout
	const vk::VkPipelineLayoutCreateInfo							pipelineLayoutInfo		=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		0u,													// VkPipelineLayoutCreateFlags	flags;
		0u,													// deUint32						descriptorSetCount;
		DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
		0u,													// deUint32						pushConstantRangeCount;
		DE_NULL												// const VkPushDescriptorRange*	pPushDescriptorRanges;
	};

	vk::PipelineLayoutWrapper										pipelineLayout			(m_pipelineConstructionType, vk, *device, &pipelineLayoutInfo);

	// Create graphics pipeline
	vk::GraphicsPipelineWrapper										graphicsPipelines[2]	{ { instanceInterface, vk, physicalDevice, *device, deviceExtensions, m_pipelineConstructionType}, { instanceInterface, vk, physicalDevice, *device, deviceExtensions, m_pipelineConstructionType} };

	const vk::VkExtent2D											extent					= {32, 32};
	const std::vector<vk::VkViewport>								viewports				(1, vk::makeViewport(extent));
	const std::vector<vk::VkRect2D>									scissors				(1, vk::makeRect2D(extent));

	for (deUint32 i = 0u; i < static_cast<deUint32>(vertexShaderModules.size()); ++i)
	{
		graphicsPipelines[i].setDefaultDepthStencilState()
			.setDefaultColorBlendState()
			.setDynamicState(&pipelineDynamicStateNfo)
			.setDefaultMultisampleState()
			.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.setDefaultRasterizationState()
			.setupVertexInputState(&vertexInputStateCreateInfo)
			.setupPreRasterizationShaderState(viewports,
				scissors,
				pipelineLayout,
				*renderPass,
				0u,
				vertexShaderModules[i])
			.setupFragmentShaderState(pipelineLayout,
				*renderPass,
				0u,
				fragmentShaderModule,
				DE_NULL,
				DE_NULL)
			.setupFragmentOutputState(*renderPass)
			.buildPipeline();
	}

	// Create vertex buffer
	const deUint32													numVertices				= 6;
	const vk::VkDeviceSize											vertexBufferSizeBytes	= 256;

	const std::array<vk::Move<vk::VkBuffer>, 2>						vertexBuffers			=
	{
		(makeBuffer(vk,*device, vertexBufferSizeBytes, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)),
		(makeBuffer(vk,*device, vertexBufferSizeBytes, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
	};

	const std::array<de::MovePtr<vk::Allocation>, 2>				vertexBufferAllocs		=
	{
		(bindBuffer(vk,*device, allocator, *vertexBuffers[0], vk::MemoryRequirement::HostVisible)),
		(bindBuffer(vk,*device, allocator, *vertexBuffers[1], vk::MemoryRequirement::HostVisible))
	};

	const deUint32													instanceSize			= (deUint32)sqrt(m_numInstances);
	const float														posIncrement			= 1.0f / (float)m_numInstances * (float)instanceSize;

	for (deUint32 i = 0u; i < static_cast<deUint32>(vertexShaderModules.size()); ++i)
	{
		tcu::Vec4			vertexColor	= (i == 0u ? tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f) : tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f));
		VertexInfo*	const	pVertices	= static_cast<VertexInfo*>(vertexBufferAllocs[i]->getHostPtr());

		pVertices[0]	= { tcu::Vec4( posIncrement, -posIncrement, 0.0f, 1.0f),	vertexColor };
		pVertices[1]	= { tcu::Vec4(-posIncrement, -posIncrement, 0.0f, 1.0f),	vertexColor };
		pVertices[2]	= { tcu::Vec4(-posIncrement,  posIncrement, 0.0f, 1.0f),	vertexColor };
		pVertices[3]	= { tcu::Vec4(-posIncrement,  posIncrement, 1.0f, 1.0f),	vertexColor };
		pVertices[4]	= { tcu::Vec4( posIncrement,  posIncrement, 1.0f, 1.0f),	vertexColor };
		pVertices[5]	= { tcu::Vec4( posIncrement, -posIncrement, 1.0f, 1.0f),	vertexColor };

		flushAlloc(vk,*device, *vertexBufferAllocs[i]);
	}

	// Command buffer
	const vk::Unique<vk::VkCommandPool>								cmdPool					(createCommandPool(vk, *device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Unique<vk::VkCommandBuffer>							cmdBuffer				(allocateCommandBuffer(vk,*device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkDeviceSize											vertexBufferOffset		= 0u;

	// Render result buffer
	const vk::VkDeviceSize											colorBufferSizeBytes	= static_cast<vk::VkDeviceSize>(tcu::getPixelSize(mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)) * 32 * 32);
	const vk::Unique<vk::VkBuffer>									colorBuffer				(makeBuffer(vk,*device, colorBufferSizeBytes, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const de::UniquePtr<vk::Allocation>								colorBufferAlloc		(bindBuffer(vk,*device, allocator, *colorBuffer, vk::MemoryRequirement::HostVisible));

	const vk::VkClearValue											clearColorValue			= defaultClearValue(vk::VK_FORMAT_R8G8B8A8_UNORM);

	beginCommandBuffer(vk, *cmdBuffer);

	renderPass.begin(vk, *cmdBuffer, vk::makeRect2D(0, 0, 32u, 32u), clearColorValue);

	graphicsPipelines[0].bind(*cmdBuffer);
	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffers[0].get(), &vertexBufferOffset);
	vk.cmdSetVertexInputEXT(*cmdBuffer, 1u, &bindingDescription2EXT, static_cast<deUint32>(vertexInputAttributeDesc2EXTGreens.size()), vertexInputAttributeDesc2EXTGreens.data());
	vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);

	graphicsPipelines[1].bind(*cmdBuffer);
	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffers[1].get(), &vertexBufferOffset);
	vk.cmdSetVertexInputEXT(*cmdBuffer, 1u, &bindingDescription2EXT, static_cast<deUint32>(vertexInputAttributeDesc2EXT2Reds.size()), vertexInputAttributeDesc2EXT2Reds.data());
	vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);

	renderPass.end(vk, *cmdBuffer);
	copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, tcu::IVec2(32, 32), vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk,*device, queue, *cmdBuffer);

	// Check result image
	{
		tcu::TextureLevel				referenceTexture	(mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM), 32, 32);
		const tcu::PixelBufferAccess	referenceAccess		= referenceTexture.getAccess();
		const int						segmentSize			= static_cast<deInt32>(32u / instanceSize);
		const int						segmentLoc			= (32 - segmentSize) / 2;

		tcu::clear(referenceTexture.getAccess(), clearColorValue.color.float32);

		// Create reference image
		for (int y = 0; y < segmentSize; ++y)
		{
			for (int x = 0; x < segmentSize; ++x)
			{
				// While running test for all offsets, we create a nice gradient-like color for the pixels.
				referenceAccess.setPixel(tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f), segmentLoc + x, segmentLoc + y);
			}
		}

		invalidateAlloc(vk,*device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess resultPixelAccess(mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM), (int)extent.width, (int)extent.height, 1, colorBufferAlloc->getHostPtr());

		if (!tcu::floatThresholdCompare(log, "color", "Image compare", referenceAccess, resultPixelAccess, tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered image is not correct");
	}

	return tcu::TestStatus::pass("Success");
}

class NonSequentialCase : public vkt::TestCase
{
public:
							NonSequentialCase	(tcu::TestContext&					testContext,
												 const std::string&					name,
												 const std::string&					description,
												 const vk::PipelineConstructionType	pipelineConstructionType,
												 const deUint32						numInstances,
												 const std::vector<deUint32>		attributeLocations)
							: vkt::TestCase					(testContext, name, description)
							, m_pipelineConstructionType	(pipelineConstructionType)
							, m_numInstances				(numInstances)
							, m_attributeLocations			(attributeLocations)
							{}

							~NonSequentialCase	(void)
							{}

	virtual void			checkSupport		(Context& context)							const override;
	virtual void			initPrograms		(vk::SourceCollections& sourceCollections)	const override;
	virtual TestInstance	*createInstance		(Context& context)							const override;

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const deUint32						m_numInstances;
	const std::vector<deUint32>			m_attributeLocations;
};

void NonSequentialCase::checkSupport (Context& context) const
{
	const vk::InstanceInterface&	vki			= context.getInstanceInterface();
	const vk::VkPhysicalDevice		physDevice	= context.getPhysicalDevice();

	std::array<std::string, 3>		extensions	=
	{
		"VK_EXT_extended_dynamic_state",
		"VK_EXT_vertex_input_dynamic_state",
		"VK_EXT_extended_dynamic_state2"
	};

	// Check extension support.
	for (const auto& extension : extensions)
		context.requireDeviceFunctionality(extension);

	vk::checkPipelineConstructionRequirements(vki, physDevice, m_pipelineConstructionType);
}

void NonSequentialCase::initPrograms (vk::SourceCollections& sourceCollections) const
{
	// Vertex
	{
		for (size_t i = 0; i < m_attributeLocations.size(); ++i)
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 inPosition;\n"
				<< "layout(location = " << m_attributeLocations[i] << ") in vec4 inColor;\n"
				<< "layout(location = 0) out vec4 outColor;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	gl_Position = inPosition;\n"
				<< "	outColor = inColor;\n"
				<< "}\n";

			sourceCollections.glslSources.add("vert_" + std::to_string(i)) << glu::VertexSource(src.str());
		}
	}

	// Fragment
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 inColor;\n"
			<< "layout(location = 0) out vec4 outColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	outColor = inColor;\n"
			<< "}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance* NonSequentialCase::createInstance (Context& context) const
{
	return new NonSequentialInstance(context, m_pipelineConstructionType, m_numInstances, m_attributeLocations);
}

} // anonymous

tcu::TestCaseGroup* createDynamicVertexAttributeTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> nonSequentialTestsGroup(new tcu::TestCaseGroup(testCtx, "dynamic_vertex_attribute", "Dynamic vertex attribute group."));

	nonSequentialTestsGroup->addChild(new NonSequentialCase(testCtx, "nonsequential", "Sequential tests.", pipelineConstructionType, 16u, { 1u, 7u }));

	return nonSequentialTestsGroup.release();
}

} // pipeline
} // vkt
