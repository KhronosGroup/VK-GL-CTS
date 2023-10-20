/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2018 ARM Limited.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Push Descriptor Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelinePushDescriptorTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMemory.h"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace std;

namespace
{
typedef vector<VkExtensionProperties>			Extensions;
typedef de::SharedPtr<Unique<VkBuffer> >		VkBufferSp;
typedef de::SharedPtr<Unique<VkImage> >			VkImageSp;
typedef de::SharedPtr<Unique<VkImageView> >		VkImageViewSp;
typedef de::SharedPtr<Unique<VkBufferView> >	VkBufferViewSp;
typedef de::SharedPtr<Allocation>				AllocationSp;
typedef de::SharedPtr<RenderPassWrapper>		VkRenderPassSp;

constexpr VkDeviceSize kSizeofVec4 = static_cast<VkDeviceSize>(sizeof(tcu::Vec4));

struct TestParams
{
	PipelineConstructionType	pipelineConstructionType;		// Used only by graphics pipeline tests
	VkDescriptorType			descriptorType;
	deUint32					binding;
	deUint32					numCalls;						// Number of draw or dispatch calls
	bool						useMaintenance5;
};

VkDeviceSize calcItemSize (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, deUint32 numElements = 1u)
{
	const auto minAlignment	= getPhysicalDeviceProperties(vki, physicalDevice).limits.minStorageBufferOffsetAlignment;
	const auto lcm			= de::lcm(de::max(VkDeviceSize{1}, minAlignment), kSizeofVec4);
	return de::roundUp(kSizeofVec4 * numElements, lcm);
}

void checkAllSupported (const Extensions& supportedExtensions, const vector<string>& requiredExtensions)
{
	for (auto& requiredExtName : requiredExtensions)
	{
		if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(requiredExtName)))
			TCU_THROW(NotSupportedError, (requiredExtName + " is not supported").c_str());
	}
}

CustomInstance createInstanceWithGetPhysicalDeviceProperties2 (Context&				context,
															   const Extensions&	supportedExtensions)
{
	vector<string> requiredExtensions = { "VK_KHR_get_physical_device_properties2" };
	checkAllSupported(supportedExtensions, requiredExtensions);

	return createCustomInstanceWithExtensions(context, requiredExtensions);
}

const char *innerCString(const string &str)
{
	return str.c_str();
}

Move<VkDevice> createDeviceWithPushDescriptor (const Context&				context,
											   const PlatformInterface&		vkp,
											   VkInstance					instance,
											   const InstanceInterface&		vki,
											   VkPhysicalDevice				physicalDevice,
											   const Extensions&			supportedExtensions,
											   const deUint32				queueFamilyIndex,
											   const TestParams&			params,
											   std::vector<std::string>&	enabledExtensions)
{

	const float						queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo	queueInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0,
		queueFamilyIndex,
		1u,
		&queuePriority
	};

	VkPhysicalDeviceFeatures		features;
	deMemset(&features, 0, sizeof(features));

	vector<string>										requiredExtensionsStr				= { "VK_KHR_push_descriptor" };
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT	graphicsPipelineLibraryFeaturesEXT	= initVulkanStructure();
	VkPhysicalDeviceDynamicRenderingFeaturesKHR			dynamicRenderingFeaturesKHR			= initVulkanStructure(&graphicsPipelineLibraryFeaturesEXT);
	VkPhysicalDeviceShaderObjectFeaturesEXT				shaderObjectFeaturesEXT				= initVulkanStructure(&dynamicRenderingFeaturesKHR);
	VkPhysicalDeviceFeatures2							features2							= initVulkanStructure(&shaderObjectFeaturesEXT);
	if (isConstructionTypeLibrary(params.pipelineConstructionType))
	{
		requiredExtensionsStr.push_back("VK_KHR_pipeline_library");
		requiredExtensionsStr.push_back("VK_EXT_graphics_pipeline_library");
		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
		if (!graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary)
			TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary required");
	}
	else if (isConstructionTypeShaderObject(params.pipelineConstructionType))
	{
		requiredExtensionsStr.push_back("VK_EXT_shader_object");
		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
		if (!shaderObjectFeaturesEXT.shaderObject)
			TCU_THROW(NotSupportedError, "shaderObjectFeaturesEXT.shaderObject required");
	}
	vector<const char *>			requiredExtensions;
	checkAllSupported(supportedExtensions, requiredExtensionsStr);
	// We need the contents of requiredExtensionsStr as a vector<const char*> in VkDeviceCreateInfo.
	transform(begin(requiredExtensionsStr), end(requiredExtensionsStr), back_inserter(requiredExtensions), innerCString);

	// Enable validation layers on this device if validation has been requested from the command line.
	const VkDeviceCreateInfo		deviceParams    =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		params.pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ? &features2 : DE_NULL,
		(VkDeviceCreateFlags)0,
		1u,
		&queueInfo,
		0u,
		DE_NULL,
		static_cast<deUint32>(requiredExtensions.size()),
		(requiredExtensions.empty() ? DE_NULL : requiredExtensions.data()),
		params.pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ? DE_NULL : &features
	};

	for (const auto& enabledExt : requiredExtensions)
		enabledExtensions.push_back(enabledExt);

	return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceParams, DE_NULL);
}

vector<Vertex4RGBA> createQuads (deUint32 numQuads, float size)
{
	vector<Vertex4RGBA>	vertices;

	for (deUint32 quadNdx = 0; quadNdx < numQuads; quadNdx++)
	{
		const float			xOffset				= -0.5f + (float)quadNdx;
		const tcu::Vec4		color				(0.0f);
		const Vertex4RGBA	lowerLeftVertex		= {tcu::Vec4(-size + xOffset, -size, 0.0f, 1.0f), color};
		const Vertex4RGBA	lowerRightVertex	= {tcu::Vec4(size + xOffset, -size, 0.0f, 1.0f), color};
		const Vertex4RGBA	UpperLeftVertex		= {tcu::Vec4(-size + xOffset, size, 0.0f, 1.0f), color};
		const Vertex4RGBA	UpperRightVertex	= {tcu::Vec4(size + xOffset, size, 0.0f, 1.0f), color};

		vertices.push_back(lowerLeftVertex);
		vertices.push_back(lowerRightVertex);
		vertices.push_back(UpperLeftVertex);
		vertices.push_back(UpperLeftVertex);
		vertices.push_back(lowerRightVertex);
		vertices.push_back(UpperRightVertex);
	}

	return vertices;
}

vector<Vertex4Tex4> createTexQuads (deUint32 numQuads, float size)
{
	vector<Vertex4Tex4>	vertices;

	for (deUint32 quadNdx = 0; quadNdx < numQuads; quadNdx++)
	{
		const float			xOffset				= -0.5f + (float)quadNdx;
		const Vertex4Tex4	lowerLeftVertex		= {tcu::Vec4(-size + xOffset, -size, 0.0f, 1.0f), tcu::Vec4(-0.2f, -0.2f, 0.0f, 0.0f)};
		const Vertex4Tex4	lowerRightVertex	= {tcu::Vec4(size + xOffset, -size, 0.0f, 1.0f), tcu::Vec4(1.2f, -0.2f, 0.0f, 0.0f)};
		const Vertex4Tex4	UpperLeftVertex		= {tcu::Vec4(-size + xOffset, size, 0.0f, 1.0f), tcu::Vec4(-0.2f, 1.2f, 0.0f, 0.0f)};
		const Vertex4Tex4	UpperRightVertex	= {tcu::Vec4(size + xOffset, size, 0.0f, 1.0f), tcu::Vec4(1.2f, 1.2f, 0.0f, 0.0f)};

		vertices.push_back(lowerLeftVertex);
		vertices.push_back(lowerRightVertex);
		vertices.push_back(UpperLeftVertex);
		vertices.push_back(UpperLeftVertex);
		vertices.push_back(lowerRightVertex);
		vertices.push_back(UpperRightVertex);
	}

	return vertices;
}

static const tcu::Vec4 defaultTestColors[] =

{
	tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
	tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)
};

class PushDescriptorBufferGraphicsTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorBufferGraphicsTestInstance	(Context& context, const TestParams& params);
	virtual						~PushDescriptorBufferGraphicsTestInstance	(void);
	void						init										(void);
	virtual tcu::TestStatus		iterate										(void);
	tcu::TestStatus				verifyImage									(void);

private:
	const TestParams				m_params;
	const PlatformInterface&		m_vkp;
	const Extensions				m_instanceExtensions;
	const CustomInstance			m_instance;
	const InstanceDriver&			m_vki;
	const VkPhysicalDevice			m_physicalDevice;
	const deUint32					m_queueFamilyIndex;
	const Extensions				m_deviceExtensions;
	std::vector<std::string>		m_deviceEnabledExtensions;
	const Unique<VkDevice>			m_device;
	const DeviceDriver				m_vkd;
	const VkQueue					m_queue;
	SimpleAllocator					m_allocator;
	const tcu::UVec2				m_renderSize;
	const VkFormat					m_colorFormat;
	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>				m_colorAttachmentView;
	RenderPassWrapper				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	ShaderWrapper					m_vertexShaderModule;
	ShaderWrapper					m_fragmentShaderModule;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;
	vector<VkBufferSp>				m_buffers;
	vector<AllocationSp>			m_bufferAllocs;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	PipelineLayoutWrapper			m_preRasterizationStatePipelineLayout;
	PipelineLayoutWrapper			m_fragmentStatePipelineLayout;
	GraphicsPipelineWrapper			m_graphicsPipeline;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	vector<Vertex4RGBA>				m_vertices;
};

PushDescriptorBufferGraphicsTestInstance::PushDescriptorBufferGraphicsTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance			(context)
	, m_params					(params)
	, m_vkp						(context.getPlatformInterface())
	, m_instanceExtensions		(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance				(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki						(m_instance.getDriver())
	, m_physicalDevice			(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex		(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions		(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device					(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue					(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_allocator				(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_renderSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_graphicsPipeline		(m_vki, m_vkd, m_physicalDevice, *m_device, m_deviceEnabledExtensions, params.pipelineConstructionType)
	, m_vertices				(createQuads(params.numCalls, 0.25f))
{
}

void PushDescriptorBufferGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	{

		const VkImageCreateInfo		colorImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			m_colorFormat,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,													// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(m_vkd, *m_device, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(m_vkd.bindImageMemory(*m_device, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(m_vkd, *m_device, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, m_colorFormat);

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			1u,											// deUint32					attachmentCount;
			attachmentBindInfos,						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32					width;
			(deUint32)m_renderSize.y(),					// deUint32					height;
			1u											// deUint32					layers;
		};

		m_renderPass.createFramebuffer(m_vkd, *m_device, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
		{
			m_params.binding,					// uint32_t				binding;
			m_params.descriptorType,			// VkDescriptorType		descriptorType;
			1u,									// uint32_t				descriptorCount;
			VK_SHADER_STAGE_VERTEX_BIT,			// VkShaderStageFlags	stageFlags;
			DE_NULL								// const VkSampler*		pImmutableSamplers;
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			1u,															// uint32_t								bindingCount;
			&descriptorSetLayoutBinding									// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		// Create pipeline layout
		VkPipelineLayoutCreateFlags	pipelineLayoutFlags = (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
		VkPipelineLayoutCreateInfo	pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			pipelineLayoutFlags,							// VkPipelineLayoutCreateFlags	flags;
			1u,												// deUint32						setLayoutCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_preRasterizationStatePipelineLayout	= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
		pipelineLayoutParams.setLayoutCount		= 0u;
		pipelineLayoutParams.pSetLayouts		= DE_NULL;
		m_fragmentStatePipelineLayout			= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create buffers. One color value in each buffer.
	{
		VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
		for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(defaultTestColors); bufIdx++)
		{
			const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			VkBufferCreateInfo	bufferCreateInfo
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u,										// VkBufferCreateFlags	flags
				kSizeofVec4,							// VkDeviceSize			size;
				usageFlags,								// VkBufferUsageFlags	usage;
				VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
				1u,										// deUint32				queueFamilyCount;
				&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
			};

			if (m_params.useMaintenance5)
			{
				bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)usageFlags;
				bufferCreateInfo.pNext = &bufferUsageFlags2;
				bufferCreateInfo.usage = 0;
			}

			m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
			m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
			VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

			deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &defaultTestColors[bufIdx], static_cast<size_t>(kSizeofVec4));
			flushAlloc(m_vkd, *m_device, *m_bufferAllocs[bufIdx]);
		}
	}

	// Create shaders
	{
		m_vertexShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("frag"), 0u);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4RGBA),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[]	=
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offsetInBytes;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// vkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPrimitiveTopology					topology							= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		const vector<VkViewport>					viewports							{ makeViewport(m_renderSize) };
		const vector<VkRect2D>						scissors							{ makeRect2D(m_renderSize) };

		m_graphicsPipeline.setDefaultRasterizationState()
						  .setDefaultDepthStencilState()
						  .setDefaultMultisampleState()
						  .setDefaultColorBlendState()
						  .setDefaultTopology(topology)
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewports,
															scissors,
															m_preRasterizationStatePipelineLayout,
															*m_renderPass,
															0u,
															m_vertexShaderModule)
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass)
						  .setMonolithicPipelineLayout(m_preRasterizationStatePipelineLayout)
						  .buildPipeline();
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&m_queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(m_vkd, *m_device, &vertexBufferParams);
		m_vertexBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(m_vkd, *m_device, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;

		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_renderPass.begin(m_vkd, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		m_graphicsPipeline.bind(*m_cmdBuffer);
		m_vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		// Draw quads. Switch input buffer which contains the quad color for each draw call.
		for (deUint32 quadNdx = 0; quadNdx < m_params.numCalls; quadNdx++)
		{
			VkDescriptorBufferInfo descriptorBufferInfo =
			{
				**m_buffers[quadNdx],	// VkBuffer			buffer;
				0u,						// VkDeviceSize		offset;
				kSizeofVec4,			// VkDeviceSize		range;
			};

			VkWriteDescriptorSet writeDescriptorSet =
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
				&descriptorBufferInfo,					// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_preRasterizationStatePipelineLayout, 0, 1, &writeDescriptorSet);
			m_vkd.cmdDraw(*m_cmdBuffer, 6, 1, 6 * quadNdx, 0);
		}

		m_renderPass.end(m_vkd, *m_cmdBuffer);
		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorBufferGraphicsTestInstance::~PushDescriptorBufferGraphicsTestInstance (void)
{
}

tcu::TestStatus PushDescriptorBufferGraphicsTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus PushDescriptorBufferGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		for (deUint32 quadIdx = 0; quadIdx < m_params.numCalls; quadIdx++)
			for (deUint32 vertexIdx = 0; vertexIdx < 6; vertexIdx++)
				m_vertices[quadIdx * 6 + vertexIdx].color.xyzw() = defaultTestColors[quadIdx];

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits),
						 rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
	}

	// Compare result with reference image
	{
		de::MovePtr<tcu::TextureLevel> result = readColorAttachment(m_vkd, *m_device, m_queue, m_queueFamilyIndex, m_allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

class PushDescriptorBufferGraphicsTest : public vkt::TestCase
{
public:
						PushDescriptorBufferGraphicsTest	(tcu::TestContext&	testContext,
															 const string&		name,
															 const string&		description,
															 const TestParams&	params);
						~PushDescriptorBufferGraphicsTest	(void);
	void				checkSupport						(Context& context) const;
	void				initPrograms						(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance						(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorBufferGraphicsTest::PushDescriptorBufferGraphicsTest (tcu::TestContext&	testContext,
																	const string&		name,
																	const string&		description,
																	const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorBufferGraphicsTest::~PushDescriptorBufferGraphicsTest (void)
{
}

TestInstance* PushDescriptorBufferGraphicsTest::createInstance (Context& context) const
{
	return new PushDescriptorBufferGraphicsTestInstance(context, m_params);
}

void PushDescriptorBufferGraphicsTest::checkSupport(Context& context) const
{
	if (m_params.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void PushDescriptorBufferGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	bufferType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? "uniform" : "readonly buffer";
	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"layout(set = 0, binding = " + de::toString(m_params.binding) + ") " + bufferType + " Block\n"
		"{\n"
		"	vec4 color;\n"
		"} inputData;\n"
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = inputData.color;\n"
		"}\n";

	const string	fragmentSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc);
	sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
}

class PushDescriptorBufferComputeTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorBufferComputeTestInstance		(Context& context, const TestParams& params);
	virtual						~PushDescriptorBufferComputeTestInstance	(void);
	void						init										(void);
	virtual tcu::TestStatus		iterate										(void);
	tcu::TestStatus				verifyOutput								(void);

private:
	const TestParams			m_params;
	const PlatformInterface&	m_vkp;
	const Extensions			m_instanceExtensions;
	const CustomInstance		m_instance;
	const InstanceDriver&		m_vki;
	const VkPhysicalDevice		m_physicalDevice;
	const deUint32				m_queueFamilyIndex;
	const Extensions			m_deviceExtensions;
	std::vector<std::string>	m_deviceEnabledExtensions;
	const Unique<VkDevice>		m_device;
	const DeviceDriver			m_vkd;
	const VkQueue				m_queue;
	const VkDeviceSize			m_itemSize;
	SimpleAllocator				m_allocator;
	Move<VkShaderModule>		m_computeShaderModule;
	vector<VkBufferSp>			m_buffers;
	vector<AllocationSp>		m_bufferAllocs;
	Move<VkBuffer>				m_outputBuffer;
	de::MovePtr<Allocation>		m_outputBufferAlloc;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_computePipeline;
	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
	std::vector<tcu::Vec4>		m_testColors;
};

PushDescriptorBufferComputeTestInstance::PushDescriptorBufferComputeTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance		(context)
	, m_params				(params)
	, m_vkp					(context.getPlatformInterface())
	, m_instanceExtensions	(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance			(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki					(m_instance.getDriver())
	, m_physicalDevice		(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_COMPUTE_BIT))
	, m_deviceExtensions	(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device				(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue				(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_itemSize			(calcItemSize(m_vki, m_physicalDevice))
	, m_allocator			(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
{
}

void PushDescriptorBufferComputeTestInstance::init (void)
{
	// Create pipeline layout
	{
		// Create descriptor set layout
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindings[]	=
		{
			{
				m_params.binding,				// uint32_t				binding;
				m_params.descriptorType,		// VkDescriptorType		descriptorType;
				1u,								// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL							// const VkSampler*		pImmutableSamplers;
			},
			{
				m_params.binding + 1,				// uint32_t				binding;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// VkDescriptorType		descriptorType;
				1u,									// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
				DE_NULL								// const VkSampler*		pImmutableSamplers;
			}
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			2u,															// uint32_t								bindingCount;
			descriptorSetLayoutBindings									// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		// Create pipeline layout
		const VkPipelineLayoutCreateInfo		pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			1u,												// deUint32						descriptorSetCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_pipelineLayout = createPipelineLayout(m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Fill the test colors table
	m_testColors.resize(m_params.numCalls);
	for (deUint32 colorIdx = 0; colorIdx < m_params.numCalls; colorIdx++)
	{
		if (colorIdx < DE_LENGTH_OF_ARRAY(defaultTestColors))
			m_testColors[colorIdx] = defaultTestColors[colorIdx];
		else
		{
			const float mix = static_cast<float>(colorIdx) / static_cast<float>(m_params.numCalls - 1);

			// interpolate between first and last color, require these colors to be different
			DE_ASSERT(defaultTestColors[0] != defaultTestColors[DE_LENGTH_OF_ARRAY(defaultTestColors) - 1]);
			m_testColors[colorIdx] = defaultTestColors[0] * mix + defaultTestColors[DE_LENGTH_OF_ARRAY(defaultTestColors) - 1] * (1.0f - mix);
		}
	}

	// Create buffers. One color value in each buffer.
	{
		for (deUint32 bufIdx = 0; bufIdx <  m_params.numCalls; bufIdx++)
		{
			const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			const VkBufferCreateInfo	bufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u,										// VkBufferCreateFlags	flags
				kSizeofVec4,							// VkDeviceSize			size;
				usageFlags,								// VkBufferUsageFlags	usage;
				VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
				1u,										// deUint32				queueFamilyCount;
				&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
			};

			m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
			m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
			VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

			deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &m_testColors[bufIdx], static_cast<size_t>(kSizeofVec4));
			flushAlloc(m_vkd, *m_device, *m_bufferAllocs[bufIdx]);
		}
	}

	// Create output buffer
	{
		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			m_itemSize * m_params.numCalls,			// VkDeviceSize			size;
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_outputBuffer		= createBuffer(m_vkd, *m_device, &bufferCreateInfo);
		m_outputBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_outputBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_outputBuffer, m_outputBufferAlloc->getMemory(), m_outputBufferAlloc->getOffset()));
	}

	// Create shader
	{
		m_computeShaderModule = createShaderModule(m_vkd, *m_device, m_context.getBinaryCollection().get("compute"), 0u);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo	stageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
			*m_computeShaderModule,									// VkShaderModule					module;
			"main",													// const char*						pName;
			DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		createInfo		=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineCreateFlags			flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo	stage;
			*m_pipelineLayout,										// VkPipelineLayout					layout;
			(VkPipeline)0,											// VkPipeline						basePipelineHandle;
			0u,														// int32_t							basePipelineIndex;
		};

		m_computePipeline = createComputePipeline(m_vkd, *m_device, (vk::VkPipelineCache)0u, &createInfo);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

		// Dispatch: Each dispatch switches the input buffer.
		// Output buffer is exposed as a vec4 sized window.
		for (deUint32 dispatchNdx = 0; dispatchNdx < m_params.numCalls; dispatchNdx++)
		{
			VkDescriptorBufferInfo descriptorBufferInfoUbo		=
			{
				**m_buffers[dispatchNdx],	// VkBuffer			buffer;
				0u,							// VkDeviceSize		offset;
				kSizeofVec4,				// VkDeviceSize		range;
			};

			VkDescriptorBufferInfo descriptorBufferInfoOutput	=
			{
				*m_outputBuffer,			// VkBuffer			buffer;
				m_itemSize * dispatchNdx,	// VkDeviceSize		offset;
				kSizeofVec4,				// VkDeviceSize		range;
			};

			VkWriteDescriptorSet writeDescriptorSets[] =
			{
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
					DE_NULL,								// const void*						pNext;
					0u,										// VkDescriptorSet					dstSet;
					m_params.binding,						// uint32_t							dstBinding;
					0u,										// uint32_t							dstArrayElement;
					1u,										// uint32_t							descriptorCount;
					m_params.descriptorType,				// VkDescriptorType					descriptorType;
					DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
					&descriptorBufferInfoUbo,				// const VkDescriptorBufferInfo*	pBufferInfo;
					DE_NULL									// const VkBufferView*				pTexelBufferView;
				},
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
					DE_NULL,								// const void*						pNext;
					0u,										// VkDescriptorSet					dstSet;
					m_params.binding + 1,					// uint32_t							dstBinding;
					0u,										// uint32_t							dstArrayElement;
					1u,										// uint32_t							descriptorCount;
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType					descriptorType;
					DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
					&descriptorBufferInfoOutput,			// const VkDescriptorBufferInfo*	pBufferInfo;
					DE_NULL									// const VkBufferView*				pTexelBufferView;
				}
			};

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 2, writeDescriptorSets);
			m_vkd.cmdDispatch(*m_cmdBuffer, 1, 1, 1);
		}

		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorBufferComputeTestInstance::~PushDescriptorBufferComputeTestInstance (void)
{
}

tcu::TestStatus PushDescriptorBufferComputeTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyOutput();
}

tcu::TestStatus PushDescriptorBufferComputeTestInstance::verifyOutput (void)
{
	invalidateAlloc(m_vkd, *m_device, *m_outputBufferAlloc);

	// Verify result
	auto bufferPtr = reinterpret_cast<const char*>(m_outputBufferAlloc->getHostPtr());
	for (deUint32 i = 0; i < m_params.numCalls; ++i)
	{
		if (deMemCmp(&m_testColors[i], bufferPtr + (i * m_itemSize), static_cast<size_t>(kSizeofVec4)) != 0)
			TCU_FAIL("Output mismatch at output item " + de::toString(i));
	}

	return tcu::TestStatus::pass("Output matches expected values");
}

class PushDescriptorBufferComputeTest : public vkt::TestCase
{
public:
						PushDescriptorBufferComputeTest		(tcu::TestContext&	testContext,
															 const string&		name,
															 const string&		description,
															 const TestParams&	params);
						~PushDescriptorBufferComputeTest	(void);
	void				initPrograms						(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance						(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorBufferComputeTest::PushDescriptorBufferComputeTest (tcu::TestContext&	testContext,
																  const string&		name,
																  const string&		description,
																  const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorBufferComputeTest::~PushDescriptorBufferComputeTest (void)
{
}

TestInstance* PushDescriptorBufferComputeTest::createInstance (Context& context) const
{
	return new PushDescriptorBufferComputeTestInstance(context, m_params);
}

void PushDescriptorBufferComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	bufferType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? "uniform" : "buffer";
	const string	computeSrc	=
		"#version 450\n"
		"layout(set = 0, binding = " + de::toString(m_params.binding) + ") " + bufferType + " Block\n"
		"{\n"
		"	vec4 color;\n"
		"} inputData;\n"
		"\n"
		"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") writeonly buffer Output\n"
		"{\n"
		"	vec4 color;\n"
		"} outData;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	outData.color = inputData.color;\n"
		"}\n";

	sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
}

class PushDescriptorImageGraphicsTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorImageGraphicsTestInstance		(Context& context, const TestParams& params);
	virtual						~PushDescriptorImageGraphicsTestInstance	(void);
	void						init										(void);
	virtual tcu::TestStatus		iterate										(void);
	tcu::TestStatus				verifyImage									(void);

private:
	const TestParams				m_params;
	const PlatformInterface&		m_vkp;
	const Extensions				m_instanceExtensions;
	const CustomInstance			m_instance;
	const InstanceDriver&			m_vki;
	const VkPhysicalDevice			m_physicalDevice;
	const deUint32					m_queueFamilyIndex;
	const Extensions				m_deviceExtensions;
	std::vector<std::string>		m_deviceEnabledExtensions;
	const Unique<VkDevice>			m_device;
	const DeviceDriver				m_vkd;
	const VkQueue					m_queue;
	SimpleAllocator					m_allocator;
	const tcu::UVec2				m_renderSize;
	const tcu::UVec2				m_textureSize;
	const VkFormat					m_colorFormat;
	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>				m_colorAttachmentView;
	vector<VkImageSp>				m_textureImages;
	vector<AllocationSp>			m_textureImageAllocs;
	vector<VkImageViewSp>			m_textureViews;
	Move<VkSampler>					m_whiteBorderSampler;
	Move<VkSampler>					m_blackBorderSampler;
	RenderPassWrapper				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	ShaderWrapper					m_vertexShaderModule;
	ShaderWrapper					m_fragmentShaderModule;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	PipelineLayoutWrapper			m_preRasterizationStatePipelineLayout;
	PipelineLayoutWrapper			m_fragmentStatePipelineLayout;
	GraphicsPipelineWrapper			m_graphicsPipeline;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	vector<Vertex4Tex4>				m_vertices;
};

PushDescriptorImageGraphicsTestInstance::PushDescriptorImageGraphicsTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance			(context)
	, m_params					(params)
	, m_vkp						(context.getPlatformInterface())
	, m_instanceExtensions		(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance				(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki						(m_instance.getDriver())
	, m_physicalDevice			(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex		(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions		(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device					(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue					(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_allocator				(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_renderSize				(32, 32)
	, m_textureSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_graphicsPipeline		(m_vki, m_vkd, m_physicalDevice, *m_device, m_deviceEnabledExtensions, params.pipelineConstructionType)
	, m_vertices				(createTexQuads(params.numCalls, 0.25f))
{
}

void PushDescriptorImageGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	{

		const VkImageCreateInfo		colorImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			m_colorFormat,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,													// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(m_vkd, *m_device, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(m_vkd.bindImageMemory(*m_device, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(m_vkd, *m_device, &colorAttachmentViewParams);
	}

	// Create texture images
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		VkImageUsageFlags			usageFlags			= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

		const VkImageCreateInfo		textureImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_colorFormat,									// VkFormat					format;
			{ m_textureSize.x(), m_textureSize.y(), 1u },	// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			usageFlags,										// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED						// VkImageLayout			initialLayout;
		};

		m_textureImages.push_back(VkImageSp(new Unique<VkImage>(createImage(m_vkd, *m_device, &textureImageParams))));

		// Allocate and bind texture image memory
		m_textureImageAllocs.push_back(AllocationSp(m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, **m_textureImages.back()), MemoryRequirement::Any).release()));
		VK_CHECK(m_vkd.bindImageMemory(*m_device, **m_textureImages.back(), m_textureImageAllocs.back()->getMemory(), m_textureImageAllocs.back()->getOffset()));
	}

	// Create texture image views
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		const VkImageViewCreateInfo textureViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			**m_textureImages[texIdx],						// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_textureViews.push_back(VkImageViewSp(new Unique<VkImageView>(createImageView(m_vkd, *m_device, &textureViewParams))));
	}

	VkClearValue	clearValues[2];
	clearValues[0].color.float32[0] = 0.0f;
	clearValues[0].color.float32[1] = 1.0f;
	clearValues[0].color.float32[2] = 0.0f;
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].color.float32[0] = 1.0f;
	clearValues[1].color.float32[1] = 0.0f;
	clearValues[1].color.float32[2] = 0.0f;
	clearValues[1].color.float32[3] = 1.0f;

	const VkImageLayout	textureImageLayout	= (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ?
											  VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Clear textures
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		const VkImageAspectFlags	aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		Move<VkCommandPool>			cmdPool;
		Move<VkCommandBuffer>		cmdBuffer;

		cmdPool		= createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);
		cmdBuffer	= allocateCommandBuffer(m_vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		const VkImageMemoryBarrier preImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			0u,										// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
			**m_textureImages[texIdx],				// VkImage					image;
			{										// VkImageSubresourceRange	subresourceRange;
				aspectMask,							// VkImageAspect			aspect;
				0u,									// deUint32					baseMipLevel;
				1u,									// deUint32					mipLevels;
				0u,									// deUint32					baseArraySlice;
				1u									// deUint32					arraySize;
			}
		};

		const VkImageMemoryBarrier	postImageBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			textureImageLayout,							// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			**m_textureImages[texIdx],					// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				aspectMask,								// VkImageAspect			aspect;
				0u,										// deUint32					baseMipLevel;
				1u,										// deUint32					mipLevels;
				0u,										// deUint32					baseArraySlice;
				1u										// deUint32					arraySize;
			}
		};

		const VkImageSubresourceRange clearRange	=
		{
			aspectMask,	// VkImageAspectFlags	aspectMask;
			0u,			// deUint32				baseMipLevel;
			1u,			// deUint32				levelCount;
			0u,			// deUint32				baseArrayLayer;
			1u			// deUint32				layerCount;
		};

		beginCommandBuffer(m_vkd, *cmdBuffer);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
		m_vkd.cmdClearColorImage(*cmdBuffer, **m_textureImages[texIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues[texIdx].color, 1, &clearRange);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		endCommandBuffer(m_vkd, *cmdBuffer);

		submitCommandsAndWait(m_vkd, *m_device, m_queue, cmdBuffer.get());
	}

	// Create samplers: one with white and one with black border color to have a visible effect on switching the sampler
	{
		VkSamplerCreateInfo samplerParams =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,							// VkFilter				magFilter;
			VK_FILTER_NEAREST,							// VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode	mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeW;
			0.0f,										// float				mipLodBias;
			VK_FALSE,									// VkBool32				anisotropyEnable;
			0.0f,										// float				maxAnisotropy;
			VK_FALSE,									// VkBool32				compareEnable;
			VK_COMPARE_OP_NEVER,						// VkCompareOp			compareOp;
			0.0f,										// float				minLod;
			0.0f,										// float				maxLod;
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,			// VkBorderColor		borderColor;
			VK_FALSE									// VkBool32				unnormalizedCoordinates;
		};

		m_whiteBorderSampler = createSampler(m_vkd, *m_device, &samplerParams);
		samplerParams.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		m_blackBorderSampler = createSampler(m_vkd, *m_device, &samplerParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription	attachmentDescription	=
		{
			(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
			VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat						format
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
		};

		const VkAttachmentReference		resultAttachmentRef		=
		{
			0u,											// deUint32			attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
		};

		const VkSubpassDescription		subpassDescription		=
		{
			(VkSubpassDescriptionFlags)0,				// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint			pipelineBindPoint
			0u,											// deUint32						inputAttachmentCount
			DE_NULL,									// const VkAttachmentReference*	pInputAttachments
			1u,											// deUint32						colorAttachmentCount
			&resultAttachmentRef,						// const VkAttachmentReference*	pColorAttachments
			DE_NULL,									// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,									// const VkAttachmentReference*	pDepthStencilAttachment
			0u,											// deUint32						preserveAttachmentCount
			DE_NULL										// const deUint32*				pPreserveAttachments
		};

		const VkRenderPassCreateInfo	renderPassInfo			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureTypei					sType
			DE_NULL,									// const void*						pNext
			(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags
			1u,											// deUint32							attachmentCount
			&attachmentDescription,						// const VkAttachmentDescription*	pAttachments
			1u,											// deUint32							subpassCount
			&subpassDescription,						// const VkSubpassDescription*		pSubpasses
			0u,											// deUint32							dependencyCount
			DE_NULL										// const VkSubpassDependency*		pDependencies
		};

		m_renderPass = RenderPassWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &renderPassInfo);
	}

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			1u,											// deUint32					attachmentCount;
			attachmentBindInfos,						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32					width;
			(deUint32)m_renderSize.y(),					// deUint32					height;
			1u											// deUint32					layers;
		};

		m_renderPass.createFramebuffer(m_vkd, *m_device, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		vector<VkDescriptorSetLayoutBinding>	layoutBindings;

		switch(m_params.descriptorType)
		{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
					{
						m_params.binding,							// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// VkDescriptorType		descriptorType;
						1u,											// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlags	stageFlags;
						DE_NULL										// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBinding);
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingSampler	=
					{
						m_params.binding,				// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLER,		// VkDescriptorType		descriptorType;
						1u,								// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
						DE_NULL							// const VkSampler*		pImmutableSamplers;
					};
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingTex	=
					{
						m_params.binding + 1,				// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBindingSampler);
					layoutBindings.push_back(descriptorSetLayoutBindingTex);
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingSampler	=
					{
						m_params.binding + 1,			// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLER,		// VkDescriptorType		descriptorType;
						1u,								// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
						DE_NULL							// const VkSampler*		pImmutableSamplers;
					};
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingTex	=
					{
						m_params.binding,					// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBindingSampler);
					layoutBindings.push_back(descriptorSetLayoutBindingTex);
				}
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
					{
						m_params.binding,					// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBinding);
				}
				break;

			default:
				DE_FATAL("unexpected descriptor type");
				break;
		}

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			(deUint32)layoutBindings.size(),							// uint32_t								bindingCount;
			layoutBindings.data()										// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		// Create pipeline layout
		VkPipelineLayoutCreateFlags	pipelineLayoutFlags = (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
		VkPipelineLayoutCreateInfo	pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			pipelineLayoutFlags,							// VkPipelineLayoutCreateFlags	flags;
			0u,												// deUint32						setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_preRasterizationStatePipelineLayout	= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
		pipelineLayoutParams.setLayoutCount		= 1u;
		pipelineLayoutParams.pSetLayouts		= &(*m_descriptorSetLayout);
		m_fragmentStatePipelineLayout			= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("frag"), 0u);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4Tex4),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[]	=
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offsetInBytes;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// vkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const vector<VkViewport>					viewports		{ makeViewport(m_renderSize) };
		const vector<VkRect2D>						scissors		{ makeRect2D(m_renderSize) };

		m_graphicsPipeline.setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
						  .setDefaultRasterizationState()
						  .setDefaultDepthStencilState()
						  .setDefaultMultisampleState()
						  .setDefaultColorBlendState()
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewports,
															scissors,
															m_preRasterizationStatePipelineLayout,
															*m_renderPass,
															0u,
															m_vertexShaderModule)
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass)
						  .buildPipeline();
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&m_queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(m_vkd, *m_device, &vertexBufferParams);
		m_vertexBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4Tex4));
		flushAlloc(m_vkd, *m_device, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;

		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_renderPass.begin(m_vkd, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		m_graphicsPipeline.bind(*m_cmdBuffer);
		m_vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		// Draw quads. Switch sampler or image view depending on the test.
		vector<VkSampler>	samplers;
		vector<VkImageView> imageViews;

		samplers.push_back(*m_whiteBorderSampler);
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		{
			// Vary sampler between draws
			samplers.push_back(*m_blackBorderSampler);
		}
		else
		{
			// Usa a single sampler
			samplers.push_back(*m_whiteBorderSampler);
		}

		imageViews.push_back(**m_textureViews[0]);
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			// Vary image view between draws
			imageViews.push_back(**m_textureViews[1]);
		}
		else
		{
			// Usa a single image view
			imageViews.push_back(**m_textureViews[0]);
		}

		for (deUint32 quadNdx = 0; quadNdx < m_params.numCalls; quadNdx++)
		{
			VkDescriptorImageInfo	descriptorImageInfo	=
			{
				samplers[quadNdx],							// VkSampler		sampler;
				imageViews[quadNdx],						// VkImageView		imageView;
				textureImageLayout							// VkImageLayout	imageLayout;
			};

			VkWriteDescriptorSet	writeDescriptorSet	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				&descriptorImageInfo,					// const VkDescriptorImageInfo*		pImageInfo;
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(writeDescriptorSet);

			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				// Sampler also needs an image.
				writeDescriptorSet.dstBinding++;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}
			else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				// Image also needs a sampler.
				writeDescriptorSet.dstBinding++;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_fragmentStatePipelineLayout, 0, (deUint32)writeDescriptorSets.size(), writeDescriptorSets.data());
			m_vkd.cmdDraw(*m_cmdBuffer, 6, 1, 6 * quadNdx, 0);
		}

		m_renderPass.end(m_vkd, *m_cmdBuffer);
		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorImageGraphicsTestInstance::~PushDescriptorImageGraphicsTestInstance (void)
{
}

tcu::TestStatus PushDescriptorImageGraphicsTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus PushDescriptorImageGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		vector<Vertex4RGBA>	refQuadsOuter	= createQuads(m_params.numCalls, 0.25f);
		vector<Vertex4RGBA>	refQuadsInner	= createQuads(m_params.numCalls, 0.25f * 0.8f);
		tcu::Vec4			outerColor[2];
		tcu::Vec4			innerColor[2];
		const bool			hasBorder		= m_params.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		if (hasBorder)
		{
			outerColor[0] = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
			innerColor[0] = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				outerColor[1] = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
			else
				outerColor[1] = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
				innerColor[1] = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
			else
				innerColor[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		}
		else
		{
			outerColor[0] = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
			outerColor[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		}

		for (deUint32 quadIdx = 0; quadIdx < m_params.numCalls; quadIdx++)
			for (deUint32 vertexIdx = 0; vertexIdx < 6; vertexIdx++)
			{
				const deUint32 idx = quadIdx * 6 + vertexIdx;
				refQuadsOuter[idx].color.xyzw() = outerColor[quadIdx];
				refQuadsInner[idx].color.xyzw() = innerColor[quadIdx];
			}

		if (hasBorder)
			refQuadsOuter.insert(refQuadsOuter.end(), refQuadsInner.begin(), refQuadsInner.end());

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits),
						 rr::PRIMITIVETYPE_TRIANGLES, refQuadsOuter);
	}

	// Compare result with reference image
	{
		de::MovePtr<tcu::TextureLevel> result = readColorAttachment(m_vkd, *m_device, m_queue, m_queueFamilyIndex, m_allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

class PushDescriptorImageGraphicsTest : public vkt::TestCase
{
public:
						PushDescriptorImageGraphicsTest		(tcu::TestContext&	testContext,
															 const string&		name,
															 const string&		description,
															 const TestParams&	params);
						~PushDescriptorImageGraphicsTest	(void);

	void				checkSupport						(Context& context) const;
	void				initPrograms						(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance						(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorImageGraphicsTest::PushDescriptorImageGraphicsTest	(tcu::TestContext&	testContext,
																	const string&		name,
																	const string&		description,
																	const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorImageGraphicsTest::~PushDescriptorImageGraphicsTest (void)
{
}

TestInstance* PushDescriptorImageGraphicsTest::createInstance (Context& context) const
{
	return new PushDescriptorImageGraphicsTestInstance(context, m_params);
}

void PushDescriptorImageGraphicsTest::checkSupport(Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void PushDescriptorImageGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 texcoordVtx;\n"
		"layout(location = 0) out highp vec2 texcoordFrag;\n"
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"	gl_Position = position;\n"
		"	texcoordFrag = texcoordVtx.xy;\n"
		"}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc);

	if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform sampler2D combinedSampler;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = texture(combinedSampler, texcoordFrag);\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
	{
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform sampler texSampler;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") uniform texture2D texImage;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = texture(sampler2D(texImage, texSampler), texcoordFrag);\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
	{
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") uniform sampler texSampler;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform texture2D texImage;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = texture(sampler2D(texImage, texSampler), texcoordFrag);\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	{
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ", rgba8) uniform readonly image2D storageImage;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = imageLoad(storageImage, ivec2(0));\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
	else
	{
		DE_FATAL("Unexpected descriptor type");
	}
}

class PushDescriptorImageComputeTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorImageComputeTestInstance	(Context& context, const TestParams& params);
	virtual						~PushDescriptorImageComputeTestInstance	(void);
	void						init									(void);
	virtual tcu::TestStatus		iterate									(void);
	tcu::TestStatus				verifyOutput							(void);

private:
	const TestParams			m_params;
	const PlatformInterface&	m_vkp;
	const Extensions			m_instanceExtensions;
	const CustomInstance		m_instance;
	const InstanceDriver&		m_vki;
	const VkPhysicalDevice		m_physicalDevice;
	const deUint32				m_queueFamilyIndex;
	const Extensions			m_deviceExtensions;
	std::vector<std::string>	m_deviceEnabledExtensions;
	const Unique<VkDevice>		m_device;
	const DeviceDriver			m_vkd;
	const VkQueue				m_queue;
	const VkDeviceSize			m_itemSize;
	const VkDeviceSize			m_blockSize;
	SimpleAllocator				m_allocator;
	const tcu::UVec2			m_textureSize;
	const VkFormat				m_colorFormat;
	Move<VkShaderModule>		m_computeShaderModule;
	vector<VkImageSp>			m_textureImages;
	vector<AllocationSp>		m_textureImageAllocs;
	vector<VkImageViewSp>		m_textureViews;
	Move<VkSampler>				m_whiteBorderSampler;
	Move<VkSampler>				m_blackBorderSampler;
	Move<VkBuffer>				m_outputBuffer;
	de::MovePtr<Allocation>		m_outputBufferAlloc;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_computePipeline;
	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
	deUint32					m_outputBufferBinding;
};

PushDescriptorImageComputeTestInstance::PushDescriptorImageComputeTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance		(context)
	, m_params				(params)
	, m_vkp					(context.getPlatformInterface())
	, m_instanceExtensions	(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance			(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki					(m_instance.getDriver())
	, m_physicalDevice		(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions	(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device				(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue				(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_itemSize			(calcItemSize(m_vki, m_physicalDevice, 2u))
	, m_blockSize			(kSizeofVec4 * 2u)
	, m_allocator			(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_textureSize			(32, 32)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_outputBufferBinding	(0)
{
}

void PushDescriptorImageComputeTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create texture images
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		VkImageUsageFlags			usageFlags			= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

		const VkImageCreateInfo		textureImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_colorFormat,									// VkFormat					format;
			{ m_textureSize.x(), m_textureSize.y(), 1u },	// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			usageFlags,										// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED						// VkImageLayout			initialLayout;
		};

		m_textureImages.push_back(VkImageSp(new Unique<VkImage>(createImage(m_vkd, *m_device, &textureImageParams))));

		// Allocate and bind texture image memory
		m_textureImageAllocs.push_back(AllocationSp(m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, **m_textureImages.back()), MemoryRequirement::Any).release()));
		VK_CHECK(m_vkd.bindImageMemory(*m_device, **m_textureImages.back(), m_textureImageAllocs.back()->getMemory(), m_textureImageAllocs.back()->getOffset()));
	}

	// Create texture image views
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		const VkImageViewCreateInfo textureViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			**m_textureImages[texIdx],						// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_textureViews.push_back(VkImageViewSp(new Unique<VkImageView>(createImageView(m_vkd, *m_device, &textureViewParams))));
	}

	VkClearValue	clearValues[2];
	clearValues[0].color.float32[0] = 0.0f;
	clearValues[0].color.float32[1] = 1.0f;
	clearValues[0].color.float32[2] = 0.0f;
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].color.float32[0] = 1.0f;
	clearValues[1].color.float32[1] = 0.0f;
	clearValues[1].color.float32[2] = 0.0f;
	clearValues[1].color.float32[3] = 1.0f;

	const VkImageLayout	textureImageLayout = (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ?
											  VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Clear textures
	for (deUint32 texIdx = 0; texIdx < 2; texIdx++)
	{
		const VkImageAspectFlags		aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		Move<VkCommandPool>				cmdPool;
		Move<VkCommandBuffer>			cmdBuffer;

		cmdPool		= createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);
		cmdBuffer	= allocateCommandBuffer(m_vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		const VkImageMemoryBarrier preImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			0u,										// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
			**m_textureImages[texIdx],				// VkImage					image;
			{										// VkImageSubresourceRange	subresourceRange;
				aspectMask,							// VkImageAspect			aspect;
				0u,									// deUint32					baseMipLevel;
				1u,									// deUint32					mipLevels;
				0u,									// deUint32					baseArraySlice;
				1u									// deUint32					arraySize;
			}
		};

		const VkImageMemoryBarrier postImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			textureImageLayout,							// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			**m_textureImages[texIdx],					// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				aspectMask,								// VkImageAspect			aspect;
				0u,										// deUint32					baseMipLevel;
				1u,										// deUint32					mipLevels;
				0u,										// deUint32					baseArraySlice;
				1u										// deUint32					arraySize;
			}
		};

		const VkImageSubresourceRange clearRange	=
		{
			aspectMask,	// VkImageAspectFlags	aspectMask;
			0u,			// deUint32				baseMipLevel;
			1u,			// deUint32				levelCount;
			0u,			// deUint32				baseArrayLayer;
			1u			// deUint32				layerCount;
		};

		beginCommandBuffer(m_vkd, *cmdBuffer);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
		m_vkd.cmdClearColorImage(*cmdBuffer, **m_textureImages[texIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues[texIdx].color, 1, &clearRange);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		endCommandBuffer(m_vkd, *cmdBuffer);

		submitCommandsAndWait(m_vkd, *m_device, m_queue, cmdBuffer.get());
	}

	// Create samplers: one with white and one with black border color to have a visible effect on switching the sampler
	{
		VkSamplerCreateInfo samplerParams =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,							// VkFilter				magFilter;
			VK_FILTER_NEAREST,							// VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode	mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeW;
			0.0f,										// float				mipLodBias;
			VK_FALSE,									// VkBool32				anisotropyEnable;
			0.0f,										// float				maxAnisotropy;
			VK_FALSE,									// VkBool32				compareEnable;
			VK_COMPARE_OP_NEVER,						// VkCompareOp			compareOp;
			0.0f,										// float				minLod;
			0.0f,										// float				maxLod;
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,			// VkBorderColor		borderColor;
			VK_FALSE									// VkBool32				unnormalizedCoordinates;
		};

		m_whiteBorderSampler = createSampler(m_vkd, *m_device, &samplerParams);
		samplerParams.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		m_blackBorderSampler = createSampler(m_vkd, *m_device, &samplerParams);
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		vector<VkDescriptorSetLayoutBinding>	layoutBindings;

		switch(m_params.descriptorType)
		{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
					{
						m_params.binding,							// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// VkDescriptorType		descriptorType;
						1u,											// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,				// VkShaderStageFlags	stageFlags;
						DE_NULL										// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBinding);
					m_outputBufferBinding = m_params.binding + 1;
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingSampler	=
					{
						m_params.binding,				// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLER,		// VkDescriptorType		descriptorType;
						1u,								// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
						DE_NULL							// const VkSampler*		pImmutableSamplers;
					};
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingTex	=
					{
						m_params.binding + 1,				// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBindingSampler);
					layoutBindings.push_back(descriptorSetLayoutBindingTex);
					m_outputBufferBinding = m_params.binding + 2;
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingSampler	=
					{
						m_params.binding + 1,			// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLER,		// VkDescriptorType		descriptorType;
						1u,								// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
						DE_NULL							// const VkSampler*		pImmutableSamplers;
					};
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingTex	=
					{
						m_params.binding,					// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBindingSampler);
					layoutBindings.push_back(descriptorSetLayoutBindingTex);
					m_outputBufferBinding = m_params.binding + 2;
				}
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				{
					const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
					{
						m_params.binding,					// uint32_t				binding;
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,	// VkDescriptorType		descriptorType;
						1u,									// uint32_t				descriptorCount;
						VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
						DE_NULL								// const VkSampler*		pImmutableSamplers;
					};
					layoutBindings.push_back(descriptorSetLayoutBinding);
					m_outputBufferBinding = m_params.binding + 1;
				}
				break;

			default:
				DE_FATAL("unexpected descriptor type");
				break;
		}

		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingOutputBuffer	=
		{
			m_outputBufferBinding,				// uint32_t				binding;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// VkDescriptorType		descriptorType;
			1u,									// uint32_t				descriptorCount;
			VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
			DE_NULL								// const VkSampler*		pImmutableSamplers;
		};

		layoutBindings.push_back(descriptorSetLayoutBindingOutputBuffer);

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			(deUint32)layoutBindings.size(),							// uint32_t								bindingCount;
			layoutBindings.data()										// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		// Create pipeline layout
		const VkPipelineLayoutCreateInfo		pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			1u,												// deUint32						descriptorSetCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_pipelineLayout = createPipelineLayout(m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create output buffer
	{
		DE_ASSERT(m_params.numCalls <= 2u);

		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			m_itemSize * 2u,						// VkDeviceSize			size;
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_outputBuffer		= createBuffer(m_vkd, *m_device, &bufferCreateInfo);
		m_outputBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_outputBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_outputBuffer, m_outputBufferAlloc->getMemory(), m_outputBufferAlloc->getOffset()));
	}

	// Create shader
	{
		m_computeShaderModule = createShaderModule(m_vkd, *m_device, m_context.getBinaryCollection().get("compute"), 0u);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo	stageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
			*m_computeShaderModule,									// VkShaderModule					module;
			"main",													// const char*						pName;
			DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		createInfo		=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineCreateFlags			flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo	stage;
			*m_pipelineLayout,										// VkPipelineLayout					layout;
			(VkPipeline)0,											// VkPipeline						basePipelineHandle;
			0u,														// int32_t							basePipelineIndex;
		};

		m_computePipeline = createComputePipeline(m_vkd, *m_device, (vk::VkPipelineCache)0u, &createInfo);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

		// Dispatch: Each dispatch switches the input image.
		// Output buffer is exposed as a 2 x vec4 sized window.
		for (deUint32 dispatchNdx = 0; dispatchNdx < m_params.numCalls; dispatchNdx++)
		{
			vector<VkSampler>	samplers;
			vector<VkImageView> imageViews;

			samplers.push_back(*m_whiteBorderSampler);
			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				// Vary sampler between draws
				samplers.push_back(*m_blackBorderSampler);
			}
			else
			{
				// Usa a single sampler
				samplers.push_back(*m_whiteBorderSampler);
			}

			imageViews.push_back(**m_textureViews[0]);
			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				// Vary image view between draws
				imageViews.push_back(**m_textureViews[1]);
			}
			else
			{
				// Usa a single image view
				imageViews.push_back(**m_textureViews[0]);
			}

			const VkDescriptorImageInfo	descriptorImageInfo	=
			{
				samplers[dispatchNdx],					// VkSampler		sampler;
				imageViews[dispatchNdx],				// VkImageView		imageView;
				textureImageLayout						// VkImageLayout	imageLayout;
			};

			VkWriteDescriptorSet	writeDescriptorSet		=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				&descriptorImageInfo,					// const VkDescriptorImageInfo*		pImageInfo;
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(writeDescriptorSet);

			if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				// Sampler also needs an image.
				writeDescriptorSet.dstBinding++;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}
			else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				// Image also needs a sampler.
				writeDescriptorSet.dstBinding++;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}

			const VkDescriptorBufferInfo descriptorBufferInfoOutput	=
			{
				*m_outputBuffer,			// VkBuffer		buffer;
				m_itemSize * dispatchNdx,	// VkDeviceSize	offset;
				m_blockSize,				// VkDeviceSize	range;
			};

			// Write output buffer descriptor set
			const VkWriteDescriptorSet	writeDescriptorSetOutput	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_outputBufferBinding,					// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType					descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
				&descriptorBufferInfoOutput,			// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			writeDescriptorSets.push_back(writeDescriptorSetOutput);

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, (deUint32)writeDescriptorSets.size(), writeDescriptorSets.data());
			m_vkd.cmdDispatch(*m_cmdBuffer, 1, 1, 1);
		}

		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorImageComputeTestInstance::~PushDescriptorImageComputeTestInstance (void)
{
}

tcu::TestStatus PushDescriptorImageComputeTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyOutput();
}

tcu::TestStatus PushDescriptorImageComputeTestInstance::verifyOutput (void)
{
	const auto			floatsPerDispatch	= 8u; // 8 floats (2 vec4s) per dispatch.
	std::vector<float>	ref					(floatsPerDispatch * 2u);

	invalidateAlloc(m_vkd, *m_device, *m_outputBufferAlloc);

	switch(m_params.descriptorType)
	{
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			// Dispatch 1: inner & outer = green
			ref[0] = ref[4] = 0.0f;
			ref[1] = ref[5] = 1.0f;
			ref[2] = ref[6] = 0.0f;
			ref[3] = ref[7] = 1.0f;

			// Dispatch 2: inner & outer = red
			ref[8] = ref[12] = 1.0f;
			ref[9] = ref[13] = 0.0f;
			ref[10] = ref[14] = 0.0f;
			ref[11] = ref[15] = 1.0f;
			break;

		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			// Dispatch 1: inner = green, outer = white
			ref[0] = 0.0f;
			ref[1] = 1.0f;
			ref[2] = 0.0f;
			ref[3] = 1.0f;

			ref[4] = 1.0f;
			ref[5] = 1.0f;
			ref[6] = 1.0f;
			ref[7] = 1.0f;

			// Dispatch 2: inner = red, outer = black
			ref[8] = 1.0f;
			ref[9] = 0.0f;
			ref[10] = 0.0f;
			ref[11] = 1.0f;

			ref[12] = 0.0f;
			ref[13] = 0.0f;
			ref[14] = 0.0f;
			ref[15] = 1.0f;
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLER:
			// Dispatch 1: inner = green, outer = white
			ref[0] = 0.0f;
			ref[1] = 1.0f;
			ref[2] = 0.0f;
			ref[3] = 1.0f;

			ref[4] = 1.0f;
			ref[5] = 1.0f;
			ref[6] = 1.0f;
			ref[7] = 1.0f;

			// Dispatch 2: inner = green, outer = black
			ref[8] = 0.0f;
			ref[9] = 1.0f;
			ref[10] = 0.0f;
			ref[11] = 1.0f;

			ref[12] = 0.0f;
			ref[13] = 0.0f;
			ref[14] = 0.0f;
			ref[15] = 1.0f;
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			// Dispatch 1: inner = green, outer = white
			ref[0] = 0.0f;
			ref[1] = 1.0f;
			ref[2] = 0.0f;
			ref[3] = 1.0f;

			ref[4] = 1.0f;
			ref[5] = 1.0f;
			ref[6] = 1.0f;
			ref[7] = 1.0f;

			// Dispatch 2: inner = red, outer = white
			ref[8] = 1.0f;
			ref[9] = 0.0f;
			ref[10] = 0.0f;
			ref[11] = 1.0f;

			ref[12] = 1.0f;
			ref[13] = 1.0f;
			ref[14] = 1.0f;
			ref[15] = 1.0f;
			break;

		default:
			DE_FATAL("unexpected descriptor type");
			break;
	}

	// Verify result
	const auto			bufferDataPtr		= reinterpret_cast<const char*>(m_outputBufferAlloc->getHostPtr());
	const auto			blockSize			= static_cast<size_t>(m_blockSize);

	for (deUint32 dispatchNdx = 0u; dispatchNdx < m_params.numCalls; ++dispatchNdx)
	{
		const auto refIdx		= floatsPerDispatch * dispatchNdx;
		const auto bufferOffset	= m_itemSize * dispatchNdx;	// Each dispatch uses m_itemSize bytes in the buffer to meet alignment reqs.

		if (deMemCmp(&ref[refIdx], bufferDataPtr + bufferOffset, blockSize) != 0)
		{
			std::vector<float> buffferValues	(floatsPerDispatch);
			std::vector<float> refValues		(floatsPerDispatch);

			deMemcpy(refValues.data(), &ref[refIdx], blockSize);
			deMemcpy(buffferValues.data(), bufferDataPtr + bufferOffset, blockSize);

			std::ostringstream msg;
			msg << "Output mismatch at dispatch " << dispatchNdx << ": Reference ";
			for (deUint32 i = 0; i < floatsPerDispatch; ++i)
				msg << ((i == 0) ? "[" : ", ") << refValues[i];
			msg << "]; Buffer ";
			for (deUint32 i = 0; i < floatsPerDispatch; ++i)
				msg << ((i == 0) ? "[" : ", ") << buffferValues[i];
			msg << "]";

			m_context.getTestContext().getLog() << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Output mismatch");
		}
	}

	return tcu::TestStatus::pass("Output matches expected values");
}

class PushDescriptorImageComputeTest : public vkt::TestCase
{
public:
						PushDescriptorImageComputeTest	(tcu::TestContext&	testContext,
														 const string&		name,
														 const string&		description,
														 const TestParams&	params);
						~PushDescriptorImageComputeTest	(void);
	void				initPrograms					(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance					(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorImageComputeTest::PushDescriptorImageComputeTest	(tcu::TestContext&	testContext,
																 const string&		name,
																 const string&		description,
																 const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorImageComputeTest::~PushDescriptorImageComputeTest (void)
{
}

TestInstance* PushDescriptorImageComputeTest::createInstance (Context& context) const
{
	return new PushDescriptorImageComputeTestInstance(context, m_params);
}

void PushDescriptorImageComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform sampler2D combinedSampler;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 innerColor;\n"
			"	vec4 outerColor;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.innerColor = texture(combinedSampler, vec2(0.5));\n"
			"	outData.outerColor = texture(combinedSampler, vec2(-0.1));\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
	{
		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform sampler texSampler;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") uniform texture2D texImage;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 2) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 innerColor;\n"
			"	vec4 outerColor;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.innerColor = texture(sampler2D(texImage, texSampler), vec2(0.5));\n"
			"	outData.outerColor = texture(sampler2D(texImage, texSampler), vec2(-0.1));\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
	{
		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") uniform sampler texSampler;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform texture2D texImage;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 2) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 innerColor;\n"
			"	vec4 outerColor;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.innerColor = texture(sampler2D(texImage, texSampler), vec2(0.5));\n"
			"	outData.outerColor = texture(sampler2D(texImage, texSampler), vec2(-0.1));\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	{
		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ", rgba8) uniform readonly image2D storageImage;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 innerColor;\n"
			"	vec4 outerColor;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.innerColor = imageLoad(storageImage, ivec2(0));\n"
			"	outData.outerColor = imageLoad(storageImage, ivec2(0));\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else
	{
		DE_FATAL("Unexpected descriptor type");
	}
}

class PushDescriptorTexelBufferGraphicsTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorTexelBufferGraphicsTestInstance	(Context& context, const TestParams& params);
	virtual						~PushDescriptorTexelBufferGraphicsTestInstance	(void);
	void						init											(void);
	virtual tcu::TestStatus		iterate											(void);
	tcu::TestStatus				verifyImage										(void);

private:
	const TestParams				m_params;
	const PlatformInterface&		m_vkp;
	const Extensions				m_instanceExtensions;
	const CustomInstance			m_instance;
	const InstanceDriver&			m_vki;
	const VkPhysicalDevice			m_physicalDevice;
	const deUint32					m_queueFamilyIndex;
	const Extensions				m_deviceExtensions;
	std::vector<std::string>		m_deviceEnabledExtensions;
	const Unique<VkDevice>			m_device;
	const DeviceDriver				m_vkd;
	const VkQueue					m_queue;
	SimpleAllocator					m_allocator;
	const tcu::UVec2				m_renderSize;
	const VkFormat					m_colorFormat;
	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>				m_colorAttachmentView;
	vector<VkBufferSp>				m_buffers;
	vector<AllocationSp>			m_bufferAllocs;
	vector<VkBufferViewSp>			m_bufferViews;
	const VkFormat					m_bufferFormat;
	RenderPassWrapper				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	ShaderWrapper					m_vertexShaderModule;
	ShaderWrapper					m_fragmentShaderModule;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	PipelineLayoutWrapper			m_preRasterizationStatePipelineLayout;
	PipelineLayoutWrapper			m_fragmentStatePipelineLayout;
	GraphicsPipelineWrapper			m_graphicsPipeline;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	vector<Vertex4RGBA>				m_vertices;
};

PushDescriptorTexelBufferGraphicsTestInstance::PushDescriptorTexelBufferGraphicsTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance			(context)
	, m_params					(params)
	, m_vkp						(context.getPlatformInterface())
	, m_instanceExtensions		(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance				(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki						(m_instance.getDriver())
	, m_physicalDevice			(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex		(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions		(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device					(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue					(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_allocator				(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_renderSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_bufferFormat			(VK_FORMAT_R32G32B32A32_SFLOAT)
	, m_graphicsPipeline		(m_vki, m_vkd, m_physicalDevice, *m_device, m_deviceEnabledExtensions, params.pipelineConstructionType)
	, m_vertices				(createQuads(params.numCalls, 0.25f))
{
}

void PushDescriptorTexelBufferGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	{

		const VkImageCreateInfo		colorImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			m_colorFormat,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,													// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(m_vkd, *m_device, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(m_vkd.bindImageMemory(*m_device, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkImageViewCreateFlags		flags;
			*m_colorImage,									// VkImage						image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType				viewType;
			m_colorFormat,									// VkFormat						format;
			componentMappingRGBA,							// VkChannelMapping				channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange		subresourceRange;
		};

		m_colorAttachmentView = createImageView(m_vkd, *m_device, &colorAttachmentViewParams);
	}

	// Create buffers
	VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
	for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(defaultTestColors); bufIdx++)
	{
		const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

		VkBufferCreateInfo	bufferCreateInfo
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			kSizeofVec4,							// VkDeviceSize			size;
			usageFlags,								// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		if (m_params.useMaintenance5)
		{
			bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)usageFlags;
			bufferCreateInfo.pNext = &bufferUsageFlags2;
			bufferCreateInfo.usage = 0;
		}

		m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
		m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
		VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

		deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &defaultTestColors[bufIdx], static_cast<size_t>(kSizeofVec4));
		flushAlloc(m_vkd, *m_device, *m_bufferAllocs[bufIdx]);
	}

	// Create buffer views
	for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(defaultTestColors); bufIdx++)
	{
		const VkBufferViewCreateInfo bufferViewParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkBufferViewCreateFlags	flags;
			**m_buffers[bufIdx],						// VkBuffer					buffer;
			m_bufferFormat,								// VkFormat					format;
			0u,											// VkDeviceSize				offset;
			VK_WHOLE_SIZE								// VkDeviceSize				range;
		};

		m_bufferViews.push_back(VkBufferViewSp(new Unique<VkBufferView>(createBufferView(m_vkd, *m_device, &bufferViewParams))));
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, m_colorFormat);

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			1u,											// deUint32					attachmentCount;
			attachmentBindInfos,						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32					width;
			(deUint32)m_renderSize.y(),					// deUint32					height;
			1u											// deUint32					layers;
		};

		m_renderPass.createFramebuffer(m_vkd, *m_device, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
		{
			m_params.binding,				// uint32_t				binding;
			m_params.descriptorType,		// VkDescriptorType		descriptorType;
			1u,								// uint32_t				descriptorCount;
			VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
			DE_NULL							// const VkSampler*		pImmutableSamplers;
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			1u,															// uint32_t								bindingCount;
			&descriptorSetLayoutBinding									// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		VkPipelineLayoutCreateFlags	pipelineLayoutFlags = (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
		VkPipelineLayoutCreateInfo	pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			pipelineLayoutFlags,							// VkPipelineLayoutCreateFlags	flags;
			0u,												// deUint32						setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_preRasterizationStatePipelineLayout	= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
		pipelineLayoutParams.setLayoutCount		= 1u;
		pipelineLayoutParams.pSetLayouts		= &(*m_descriptorSetLayout);
		m_fragmentStatePipelineLayout			= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("frag"), 0u);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4RGBA),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[]	=
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offsetInBytes;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color)	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// vkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const vector<VkViewport>					viewports			{ makeViewport(m_renderSize) };
		const vector<VkRect2D>						scissors			{ makeRect2D(m_renderSize) };

		m_graphicsPipeline.setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
						  .setDefaultRasterizationState()
						  .setDefaultDepthStencilState()
						  .setDefaultMultisampleState()
						  .setDefaultColorBlendState()
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewports,
															scissors,
															m_preRasterizationStatePipelineLayout,
															*m_renderPass,
															0u,
															m_vertexShaderModule)
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass)
						  .buildPipeline();
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&m_queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(m_vkd, *m_device, &vertexBufferParams);
		m_vertexBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(m_vkd, *m_device, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;

		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_renderPass.begin(m_vkd, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		m_graphicsPipeline.bind(*m_cmdBuffer);
		m_vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		// Draw quads. Switch buffer view between draws.
		for (deUint32 quadNdx = 0; quadNdx < m_params.numCalls; quadNdx++)
		{
			VkWriteDescriptorSet	writeDescriptorSet	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo;
				&m_bufferViews[quadNdx]->get()			// const VkBufferView*				pTexelBufferView;
			};

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_fragmentStatePipelineLayout, 0, 1u, &writeDescriptorSet);
			m_vkd.cmdDraw(*m_cmdBuffer, 6, 1, 6 * quadNdx, 0);
		}

		m_renderPass.end(m_vkd, *m_cmdBuffer);
		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorTexelBufferGraphicsTestInstance::~PushDescriptorTexelBufferGraphicsTestInstance (void)
{
}

tcu::TestStatus PushDescriptorTexelBufferGraphicsTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus PushDescriptorTexelBufferGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		for (deUint32 quadIdx = 0; quadIdx < m_params.numCalls; quadIdx++)
			for (deUint32 vertexIdx = 0; vertexIdx < 6; vertexIdx++)
				m_vertices[quadIdx * 6 + vertexIdx].color.xyzw() = defaultTestColors[quadIdx];

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits),
						 rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
	}

	// Compare result with reference image
	{
		de::MovePtr<tcu::TextureLevel> result = readColorAttachment(m_vkd, *m_device, m_queue, m_queueFamilyIndex, m_allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

class PushDescriptorTexelBufferGraphicsTest : public vkt::TestCase
{
public:
						PushDescriptorTexelBufferGraphicsTest	(tcu::TestContext&	testContext,
																 const string&		name,
																 const string&		description,
																 const TestParams&	params);
						~PushDescriptorTexelBufferGraphicsTest	(void);

	void				checkSupport						(Context& context) const;
	void				initPrograms						(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance						(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorTexelBufferGraphicsTest::PushDescriptorTexelBufferGraphicsTest	(tcu::TestContext&	testContext,
																				 const string&		name,
																				 const string&		description,
																				 const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorTexelBufferGraphicsTest::~PushDescriptorTexelBufferGraphicsTest (void)
{
}

TestInstance* PushDescriptorTexelBufferGraphicsTest::createInstance (Context& context) const
{
	return new PushDescriptorTexelBufferGraphicsTestInstance(context, m_params);
}

void PushDescriptorTexelBufferGraphicsTest::checkSupport(Context& context) const
{
	if (m_params.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void PushDescriptorTexelBufferGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 texcoordVtx;\n"
		"layout(location = 0) out highp vec2 texcoordFrag;\n"
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"	gl_Position = position;\n"
		"	texcoordFrag = texcoordVtx.xy;\n"
		"}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc);

	if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
	{
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform textureBuffer texelBuffer;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = texelFetch(texelBuffer, 0);\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
	else
	{
		DE_ASSERT(m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
		const string	fragmentSrc	=
			"#version 450\n"
			"layout(location = 0) in highp vec2 texcoordFrag;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ", rgba32f) uniform readonly imageBuffer texelBuffer;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = imageLoad(texelBuffer, 0);\n"
			"}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
	}
}

class PushDescriptorTexelBufferComputeTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorTexelBufferComputeTestInstance	(Context& context, const TestParams& params);
	virtual						~PushDescriptorTexelBufferComputeTestInstance	(void);
	void						init											(void);
	virtual tcu::TestStatus		iterate											(void);
	tcu::TestStatus				verifyOutput									(void);

private:
	const TestParams			m_params;
	const PlatformInterface&	m_vkp;
	const Extensions			m_instanceExtensions;
	const CustomInstance		m_instance;
	const InstanceDriver&		m_vki;
	const VkPhysicalDevice		m_physicalDevice;
	const deUint32				m_queueFamilyIndex;
	const Extensions			m_deviceExtensions;
	std::vector<std::string>	m_deviceEnabledExtensions;
	const Unique<VkDevice>		m_device;
	const DeviceDriver			m_vkd;
	const VkQueue				m_queue;
	const VkDeviceSize			m_itemSize;
	SimpleAllocator				m_allocator;
	vector<VkBufferSp>			m_buffers;
	vector<AllocationSp>		m_bufferAllocs;
	vector<VkBufferViewSp>		m_bufferViews;
	const VkFormat				m_bufferFormat;
	Move<VkShaderModule>		m_computeShaderModule;
	Move<VkBuffer>				m_outputBuffer;
	de::MovePtr<Allocation>		m_outputBufferAlloc;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_computePipeline;
	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
};

PushDescriptorTexelBufferComputeTestInstance::PushDescriptorTexelBufferComputeTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance		(context)
	, m_params				(params)
	, m_vkp					(context.getPlatformInterface())
	, m_instanceExtensions	(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance			(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki					(m_instance.getDriver())
	, m_physicalDevice		(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_COMPUTE_BIT))
	, m_deviceExtensions	(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device				(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue				(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_itemSize			(calcItemSize(m_vki, m_physicalDevice))
	, m_allocator			(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_bufferFormat		(VK_FORMAT_R32G32B32A32_SFLOAT)
{
}

void PushDescriptorTexelBufferComputeTestInstance::init (void)
{
	// Create buffers
	for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(defaultTestColors); bufIdx++)
	{
		const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			kSizeofVec4,							// VkDeviceSize			size;
			usageFlags,								// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
		m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
		VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

		deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &defaultTestColors[bufIdx], static_cast<size_t>(kSizeofVec4));
		flushAlloc(m_vkd, *m_device, *m_bufferAllocs[bufIdx]);
	}

	// Create buffer views
	for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(defaultTestColors); bufIdx++)
	{
		const VkBufferViewCreateInfo bufferViewParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkBufferViewCreateFlags	flags;
			**m_buffers[bufIdx],						// VkBuffer					buffer;
			m_bufferFormat,								// VkFormat					format;
			0u,											// VkDeviceSize				offset;
			VK_WHOLE_SIZE								// VkDeviceSize				range;
		};

		m_bufferViews.push_back(VkBufferViewSp(new Unique<VkBufferView>(createBufferView(m_vkd, *m_device, &bufferViewParams))));
	}

	// Create pipeline layout
	{
		vector<VkDescriptorSetLayoutBinding>	layoutBindings;

		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindings[]		=
		{
			{
				m_params.binding,				// uint32_t				binding;
				m_params.descriptorType,		// VkDescriptorType		descriptorType;
				1u,								// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL							// const VkSampler*		pImmutableSamplers;
			},
			{
				m_params.binding + 1,				// uint32_t				binding;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// VkDescriptorType		descriptorType;
				1u,									// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags;
				DE_NULL								// const VkSampler*		pImmutableSamplers;
			}
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			2u,															// uint32_t								bindingCount;
			descriptorSetLayoutBindings									// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			1u,												// deUint32						descriptorSetCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_pipelineLayout = createPipelineLayout(m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create output buffer
	{
		DE_ASSERT(m_params.numCalls <= 2u);

		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			m_itemSize * m_params.numCalls,			// VkDeviceSize			size;
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_outputBuffer		= createBuffer(m_vkd, *m_device, &bufferCreateInfo);
		m_outputBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_outputBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_outputBuffer, m_outputBufferAlloc->getMemory(), m_outputBufferAlloc->getOffset()));
	}

	// Create shader
	{
		m_computeShaderModule = createShaderModule(m_vkd, *m_device, m_context.getBinaryCollection().get("compute"), 0u);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo	stageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
			*m_computeShaderModule,									// VkShaderModule					module;
			"main",													// const char*						pName;
			DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		createInfo		=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineCreateFlags			flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo	stage;
			*m_pipelineLayout,										// VkPipelineLayout					layout;
			(VkPipeline)0,											// VkPipeline						basePipelineHandle;
			0u,														// int32_t							basePipelineIndex;
		};

		m_computePipeline = createComputePipeline(m_vkd, *m_device, (vk::VkPipelineCache)0u, &createInfo);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		m_vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

		// Dispatch: Each dispatch switches the input image.
		// Output buffer is exposed as a vec4 sized window.
		for (deUint32 dispatchNdx = 0; dispatchNdx < m_params.numCalls; dispatchNdx++)
		{
			VkWriteDescriptorSet	writeDescriptorSet	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo;
				&m_bufferViews[dispatchNdx]->get()		// const VkBufferView*				pTexelBufferView;
			};

			vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(writeDescriptorSet);

			const VkDescriptorBufferInfo descriptorBufferInfoOutput	=
			{
				*m_outputBuffer,			// VkBuffer			buffer;
				m_itemSize * dispatchNdx,	// VkDeviceSize		offset;
				kSizeofVec4,				// VkDeviceSize		range;
			};

			// Write output buffer descriptor set
			const VkWriteDescriptorSet	writeDescriptorSetOutput	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding + 1,					// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType					descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
				&descriptorBufferInfoOutput,			// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			writeDescriptorSets.push_back(writeDescriptorSetOutput);

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, (deUint32)writeDescriptorSets.size(), writeDescriptorSets.data());
			m_vkd.cmdDispatch(*m_cmdBuffer, 1, 1, 1);
		}

		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorTexelBufferComputeTestInstance::~PushDescriptorTexelBufferComputeTestInstance (void)
{
}

tcu::TestStatus PushDescriptorTexelBufferComputeTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyOutput();
}

tcu::TestStatus PushDescriptorTexelBufferComputeTestInstance::verifyOutput (void)
{
	const tcu::Vec4 ref[2] = { { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } };
	invalidateAlloc(m_vkd, *m_device, *m_outputBufferAlloc);

	// Verify result
	DE_ASSERT(m_params.numCalls <= 2u);

	auto bufferPtr = reinterpret_cast<const char*>(m_outputBufferAlloc->getHostPtr());
	for (deUint32 i = 0; i < m_params.numCalls; ++i)
	{
		tcu::Vec4 bufferColor;
		deMemcpy(&bufferColor, bufferPtr + (i * m_itemSize), static_cast<size_t>(kSizeofVec4));

		if (bufferColor != ref[i])
		{
			std::ostringstream msg;
			msg << "Output mismatch at item " << i << ": expected " << ref[i] << " but found " << bufferColor;
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Output matches expected values");
}

class PushDescriptorTexelBufferComputeTest : public vkt::TestCase
{
public:
						PushDescriptorTexelBufferComputeTest	(tcu::TestContext&	testContext,
																 const string&		name,
																 const string&		description,
																 const TestParams&	params);
						~PushDescriptorTexelBufferComputeTest	(void);
	void				initPrograms							(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance							(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorTexelBufferComputeTest::PushDescriptorTexelBufferComputeTest	(tcu::TestContext&	testContext,
																			 const string&		name,
																			 const string&		description,
																			 const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorTexelBufferComputeTest::~PushDescriptorTexelBufferComputeTest (void)
{
}

TestInstance* PushDescriptorTexelBufferComputeTest::createInstance (Context& context) const
{
	return new PushDescriptorTexelBufferComputeTestInstance(context, m_params);
}

void PushDescriptorTexelBufferComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
	{
		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ") uniform textureBuffer texelBuffer;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 color;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.color = texelFetch(texelBuffer, 0);\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else
	{
		DE_ASSERT(m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);

		const string	computeSrc	=
			"#version 450\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding) + ", rgba32f) uniform readonly imageBuffer texelBuffer;\n"
			"layout(set = 0, binding = " + de::toString(m_params.binding + 1) + ") writeonly buffer Output\n"
			"{\n"
			"	vec4 color;\n"
			"} outData;\n"
			"\n"
			"void main()\n"
			"{\n"
			"	outData.color = imageLoad(texelBuffer, 0);\n"
			"}\n";

		sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
}

class PushDescriptorInputAttachmentGraphicsTestInstance : public vkt::TestInstance
{
public:
								PushDescriptorInputAttachmentGraphicsTestInstance	(Context& context, const TestParams& params);
	virtual						~PushDescriptorInputAttachmentGraphicsTestInstance	(void);
	void						init												(void);
	virtual tcu::TestStatus		iterate												(void);
	tcu::TestStatus				verifyImage											(void);

private:
	const TestParams				m_params;
	const PlatformInterface&		m_vkp;
	const Extensions				m_instanceExtensions;
	const CustomInstance			m_instance;
	const InstanceDriver&			m_vki;
	const VkPhysicalDevice			m_physicalDevice;
	const deUint32					m_queueFamilyIndex;
	const Extensions				m_deviceExtensions;
	std::vector<std::string>		m_deviceEnabledExtensions;
	const Unique<VkDevice>			m_device;
	const DeviceDriver				m_vkd;
	const VkQueue					m_queue;
	SimpleAllocator					m_allocator;
	const tcu::UVec2				m_renderSize;
	const tcu::UVec2				m_textureSize;
	const VkFormat					m_colorFormat;
	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>				m_colorAttachmentView;
	vector<VkImageSp>				m_inputImages;
	vector<AllocationSp>			m_inputImageAllocs;
	vector<VkImageViewSp>			m_inputImageViews;
	vector<VkRenderPassSp>			m_renderPasses;
	ShaderWrapper					m_vertexShaderModule;
	ShaderWrapper					m_fragmentShaderModule;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	PipelineLayoutWrapper			m_preRasterizationStatePipelineLayout;
	PipelineLayoutWrapper			m_fragmentStatePipelineLayout;
	vector<GraphicsPipelineWrapper>	m_graphicsPipelines;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	vector<Vertex4Tex4>				m_vertices;
};

PushDescriptorInputAttachmentGraphicsTestInstance::PushDescriptorInputAttachmentGraphicsTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance			(context)
	, m_params					(params)
	, m_vkp						(context.getPlatformInterface())
	, m_instanceExtensions		(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance				(createInstanceWithGetPhysicalDeviceProperties2(context, m_instanceExtensions))
	, m_vki						(m_instance.getDriver())
	, m_physicalDevice			(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex		(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions		(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device					(createDeviceWithPushDescriptor(context, m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, params, m_deviceEnabledExtensions))
	, m_vkd					(m_vkp, m_instance, *m_device, context.getUsedApiVersion())
	, m_queue					(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_allocator				(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_renderSize				(32, 32)
	, m_textureSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices				(createTexQuads(params.numCalls, 0.25f))
{
}

void PushDescriptorInputAttachmentGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	{

		const VkImageCreateInfo		colorImageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			m_colorFormat,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,													// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		m_colorImage		= createImage(m_vkd, *m_device, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc	= m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(m_vkd.bindImageMemory(*m_device, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(m_vkd, *m_device, &colorAttachmentViewParams);
	}

	// Create input images
	for (deUint32 imageIdx = 0; imageIdx < 2; imageIdx++)
	{
		const VkImageUsageFlags		usageFlags			= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		const VkImageCreateInfo		inputImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_colorFormat,									// VkFormat					format;
			{ m_textureSize.x(), m_textureSize.y(), 1u },	// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			usageFlags,										// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&m_queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED						// VkImageLayout			initialLayout;
		};

		m_inputImages.push_back(VkImageSp(new Unique<VkImage>(createImage(m_vkd, *m_device, &inputImageParams))));

		// Allocate and bind image memory
		m_inputImageAllocs.push_back(AllocationSp(m_allocator.allocate(getImageMemoryRequirements(m_vkd, *m_device, **m_inputImages.back()), MemoryRequirement::Any).release()));
		VK_CHECK(m_vkd.bindImageMemory(*m_device, **m_inputImages.back(), m_inputImageAllocs.back()->getMemory(), m_inputImageAllocs.back()->getOffset()));
	}

	// Create texture image views
	for (deUint32 imageIdx = 0; imageIdx < 2; imageIdx++)
	{
		const VkImageViewCreateInfo textureViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			**m_inputImages[imageIdx],						// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_inputImageViews.push_back(VkImageViewSp(new Unique<VkImageView>(createImageView(m_vkd, *m_device, &textureViewParams))));
	}

	VkClearValue clearValues[2];
	clearValues[0].color.float32[0] = 0.0f;
	clearValues[0].color.float32[1] = 1.0f;
	clearValues[0].color.float32[2] = 0.0f;
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].color.float32[0] = 1.0f;
	clearValues[1].color.float32[1] = 0.0f;
	clearValues[1].color.float32[2] = 0.0f;
	clearValues[1].color.float32[3] = 1.0f;

	// Clear input images
	for (deUint32 imageIdx = 0; imageIdx < 2; imageIdx++)
	{
		const VkImageAspectFlags	aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		Move<VkCommandPool>			cmdPool;
		Move<VkCommandBuffer>		cmdBuffer;
		const VkAccessFlags			accessFlags	= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

		cmdPool		= createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);
		cmdBuffer	= allocateCommandBuffer(m_vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		const VkImageMemoryBarrier preImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			0u,										// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
			**m_inputImages[imageIdx],				// VkImage					image;
			{										// VkImageSubresourceRange	subresourceRange;
				aspectMask,							// VkImageAspect			aspect;
				0u,									// deUint32					baseMipLevel;
				1u,									// deUint32					mipLevels;
				0u,									// deUint32					baseArraySlice;
				1u									// deUint32					arraySize;
			}
		};

		const VkImageMemoryBarrier postImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
			accessFlags,								// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			**m_inputImages[imageIdx],					// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				aspectMask,								// VkImageAspect			aspect;
				0u,										// deUint32					baseMipLevel;
				1u,										// deUint32					mipLevels;
				0u,										// deUint32					baseArraySlice;
				1u										// deUint32					arraySize;
			}
		};

		const VkImageSubresourceRange clearRange	=
		{
			aspectMask,	// VkImageAspectFlags	aspectMask;
			0u,			// deUint32				baseMipLevel;
			1u,			// deUint32				levelCount;
			0u,			// deUint32				baseArrayLayer;
			1u			// deUint32				layerCount;
		};

		beginCommandBuffer(m_vkd, *cmdBuffer);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
		m_vkd.cmdClearColorImage(*cmdBuffer, **m_inputImages[imageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues[imageIdx].color, 1, &clearRange);
		m_vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
		endCommandBuffer(m_vkd, *cmdBuffer);

		submitCommandsAndWait(m_vkd, *m_device, m_queue, cmdBuffer.get());
	}

	// Create render passes
	for (deUint32 renderPassIdx = 0; renderPassIdx < 2; renderPassIdx++)
	{
		// The first pass clears the output image, and the second one draws on top of the first pass.
		const VkAttachmentLoadOp		loadOps[]					=
		{
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_LOAD_OP_LOAD
		};

		const VkImageLayout				initialLayouts[]			=
		{
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		const VkAttachmentDescription	attachmentDescriptions[]	=
		{
			// Result attachment
			{
				(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
				VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat						format
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
				loadOps[renderPassIdx],						// VkAttachmentLoadOp			loadOp
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
				initialLayouts[renderPassIdx],				// VkImageLayout				initialLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
			},
			// Input attachment
			{
				(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
				VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat						format
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
				VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp			loadOp
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout				initialLayout
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout				finalLayout
			}
		};

		const VkAttachmentReference		resultAttachmentRef		=
		{
			0u,											// deUint32			attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
		};

		const VkAttachmentReference		inputAttachmentRef		=
		{
			1u,											// deUint32			attachment
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	layout
		};

		const VkSubpassDescription		subpassDescription		=
		{
			(VkSubpassDescriptionFlags)0,				// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint			pipelineBindPoint
			1u,											// deUint32						inputAttachmentCount
			&inputAttachmentRef,							// const VkAttachmentReference*	pInputAttachments
			1u,											// deUint32						colorAttachmentCount
			&resultAttachmentRef,						// const VkAttachmentReference*	pColorAttachments
			DE_NULL,									// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,									// const VkAttachmentReference*	pDepthStencilAttachment
			0u,											// deUint32						preserveAttachmentCount
			DE_NULL										// const deUint32*				pPreserveAttachments
		};

		const VkSubpassDependency		subpassDependency		=
		{
			VK_SUBPASS_EXTERNAL,							// deUint32				srcSubpass
			0,												// deUint32				dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags	srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			// VkPipelineStageFlags	dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags		srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT,	//	dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT						// VkDependencyFlags	dependencyFlags
		};

		const VkRenderPassCreateInfo	renderPassInfo			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureTypei					sType
			DE_NULL,									// const void*						pNext
			(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags
			2u,											// deUint32							attachmentCount
			attachmentDescriptions,						// const VkAttachmentDescription*	pAttachments
			1u,											// deUint32							subpassCount
			&subpassDescription,						// const VkSubpassDescription*		pSubpasses
			1u,											// deUint32							dependencyCount
			&subpassDependency							// const VkSubpassDependency*		pDependencies
		};

		m_renderPasses.push_back(VkRenderPassSp(new RenderPassWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &renderPassInfo)));

		std::vector<VkImage>			images					=
		{
			*m_colorImage,
			**m_inputImages[renderPassIdx],
		};

		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView,
			**m_inputImageViews[renderPassIdx],
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			**m_renderPasses[renderPassIdx],			// VkRenderPass				renderPass;
			2u,											// deUint32					attachmentCount;
			attachmentBindInfos,						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32					width;
			(deUint32)m_renderSize.y(),					// deUint32					height;
			1u											// deUint32					layers;
		};

		m_renderPasses[renderPassIdx]->createFramebuffer(m_vkd, *m_device, &framebufferParams, images);
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
		{
			m_params.binding,						// uint32_t				binding;
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType		descriptorType;
			1u,										// uint32_t				descriptorCount;
			VK_SHADER_STAGE_FRAGMENT_BIT,			// VkShaderStageFlags	stageFlags;
			DE_NULL									// const VkSampler*		pImmutableSamplers;
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags		flags;
			1u,															// uint32_t								bindingCount;
			&descriptorSetLayoutBinding									// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(m_vkd, *m_device, &descriptorSetLayoutCreateInfo, DE_NULL);

		// Create pipeline layout
		VkPipelineLayoutCreateFlags	pipelineLayoutFlags = (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
		VkPipelineLayoutCreateInfo	pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			pipelineLayoutFlags,							// VkPipelineLayoutCreateFlags	flags;
			0u,												// deUint32						setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_preRasterizationStatePipelineLayout	= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
		pipelineLayoutParams.setLayoutCount		= 1u;
		pipelineLayoutParams.pSetLayouts		= &(*m_descriptorSetLayout);
		m_fragmentStatePipelineLayout			= PipelineLayoutWrapper(m_params.pipelineConstructionType, m_vkd, *m_device, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= ShaderWrapper(m_vkd, *m_device, m_context.getBinaryCollection().get("frag"), 0u);
	}

	m_graphicsPipelines.reserve(2);

	// Create pipelines
	for (deUint32 pipelineIdx = 0; pipelineIdx < 2; pipelineIdx++)
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4Tex4),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[]	=
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offsetInBytes;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// vkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const vector<VkViewport>	viewports	{ makeViewport(m_renderSize) };
		const vector<VkRect2D>		scissors	{ makeRect2D(m_renderSize) };

		m_graphicsPipelines.emplace_back(m_vki, m_vkd, m_physicalDevice, *m_device, m_deviceEnabledExtensions, m_params.pipelineConstructionType);
		m_graphicsPipelines.back().setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
								  .setDefaultRasterizationState()
								  .setDefaultDepthStencilState()
								  .setDefaultMultisampleState()
								  .setDefaultColorBlendState()
								  .setupVertexInputState(&vertexInputStateParams)
								  .setupPreRasterizationShaderState(viewports,
																	scissors,
																	m_preRasterizationStatePipelineLayout,
																	**m_renderPasses[pipelineIdx],
																	0u,
																	m_vertexShaderModule)
								  .setupFragmentShaderState(m_fragmentStatePipelineLayout, **m_renderPasses[pipelineIdx], 0u, m_fragmentShaderModule)
								  .setupFragmentOutputState(**m_renderPasses[pipelineIdx])
								  .buildPipeline();
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&m_queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(m_vkd, *m_device, &vertexBufferParams);
		m_vertexBufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_vkd.bindBufferMemory(*m_device, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4Tex4));
		flushAlloc(m_vkd, *m_device, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;

		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		for (deUint32 quadNdx = 0; quadNdx < m_params.numCalls; quadNdx++)
		{
			(*m_renderPasses[quadNdx]).begin(m_vkd, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
			m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);
			m_vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

			VkDescriptorImageInfo	descriptorImageInfo	=
			{
				0,											// VkSampler		sampler;
				**m_inputImageViews[quadNdx],				// VkImageView		imageView;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
			};

			VkWriteDescriptorSet	writeDescriptorSet	=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
				DE_NULL,								// const void*						pNext;
				0u,										// VkDescriptorSet					dstSet;
				m_params.binding,						// uint32_t							dstBinding;
				0u,										// uint32_t							dstArrayElement;
				1u,										// uint32_t							descriptorCount;
				m_params.descriptorType,				// VkDescriptorType					descriptorType;
				&descriptorImageInfo,					// const VkDescriptorImageInfo*		pImageInfo;
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo;
				DE_NULL									// const VkBufferView*				pTexelBufferView;
			};

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_fragmentStatePipelineLayout, 0, 1, &writeDescriptorSet);
			m_vkd.cmdDraw(*m_cmdBuffer, 6, 1, 6 * quadNdx, 0);

			(*m_renderPasses[quadNdx]).end(m_vkd, *m_cmdBuffer);
		}

		endCommandBuffer(m_vkd, *m_cmdBuffer);
	}
}

PushDescriptorInputAttachmentGraphicsTestInstance::~PushDescriptorInputAttachmentGraphicsTestInstance (void)
{
}

tcu::TestStatus PushDescriptorInputAttachmentGraphicsTestInstance::iterate (void)
{
	init();

	submitCommandsAndWait(m_vkd, *m_device, m_queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus PushDescriptorInputAttachmentGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		vector<Vertex4RGBA>	refQuads = createQuads(m_params.numCalls, 0.25f);
		tcu::Vec4			colors[2];

		colors[0] = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
		colors[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);

		for (deUint32 quadIdx = 0; quadIdx < m_params.numCalls; quadIdx++)
			for (deUint32 vertexIdx = 0; vertexIdx < 6; vertexIdx++)
			{
				const deUint32 idx = quadIdx * 6 + vertexIdx;
				refQuads[idx].color.xyzw() = colors[quadIdx];
			}

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits),
						 rr::PRIMITIVETYPE_TRIANGLES, refQuads);
	}

	// Compare result with reference image
	{
		de::MovePtr<tcu::TextureLevel> result = readColorAttachment(m_vkd, *m_device, m_queue, m_queueFamilyIndex, m_allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

class PushDescriptorInputAttachmentGraphicsTest : public vkt::TestCase
{
public:
						PushDescriptorInputAttachmentGraphicsTest		(tcu::TestContext&	testContext,
															 const string&		name,
															 const string&		description,
															 const TestParams&	params);
						~PushDescriptorInputAttachmentGraphicsTest	(void);

	void				checkSupport						(Context& context) const;
	void				initPrograms						(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance						(Context& context) const;

protected:
	const TestParams	m_params;
};

PushDescriptorInputAttachmentGraphicsTest::PushDescriptorInputAttachmentGraphicsTest	(tcu::TestContext&	testContext,
																	const string&		name,
																	const string&		description,
																	const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

PushDescriptorInputAttachmentGraphicsTest::~PushDescriptorInputAttachmentGraphicsTest (void)
{
}

TestInstance* PushDescriptorInputAttachmentGraphicsTest::createInstance (Context& context) const
{
	return new PushDescriptorInputAttachmentGraphicsTestInstance(context, m_params);
}

void PushDescriptorInputAttachmentGraphicsTest::checkSupport(Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void PushDescriptorInputAttachmentGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 texcoordVtx;\n"
		"layout(location = 0) out highp vec2 texcoordFrag;\n"
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"	gl_Position = position;\n"
		"	texcoordFrag = texcoordVtx.xy;\n"
		"}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc);

	const string	fragmentSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec2 texcoordFrag;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"layout(input_attachment_index = 0, set = 0, binding = " + de::toString(m_params.binding) + ") uniform subpassInput inputColor;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = subpassLoad(inputColor);\n"
		"}\n";

	sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
}

} // anonymous

tcu::TestCaseGroup* createPushDescriptorTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineType)
{
	const TestParams params[]
	{
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				1u, 128u,	false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLER,						0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLER,						0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLER,						1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLER,						3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		3u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			0u, 1u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			0u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			1u, 2u,		false },
		{ pipelineType, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			3u, 2u,		false }
	};

	de::MovePtr<tcu::TestCaseGroup>	pushDescriptorTests	(new tcu::TestCaseGroup(testCtx, "push_descriptor", "Push descriptor tests"));

	de::MovePtr<tcu::TestCaseGroup>	graphicsTests		(new tcu::TestCaseGroup(testCtx, "graphics", "graphics pipeline"));
	de::MovePtr<tcu::TestCaseGroup>	computeTests		(new tcu::TestCaseGroup(testCtx, "compute", "compute pipeline"));

	for (deUint32 testIdx = 0; testIdx < DE_LENGTH_OF_ARRAY(params); testIdx++)
	{
		string testName;
		testName += "binding" + de::toString(params[testIdx].binding) + "_numcalls" + de::toString(params[testIdx].numCalls);
		switch(params[testIdx].descriptorType)
		{
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				testName += "_uniform_buffer";
				if (params[testIdx].numCalls <= 2)
					graphicsTests->addChild(new PushDescriptorBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				testName += "_storage_buffer";
				if (params[testIdx].numCalls <= 2)
					graphicsTests->addChild(new PushDescriptorBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				testName += "_combined_image_sampler";
				graphicsTests->addChild(new PushDescriptorImageGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorImageComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				testName += "_sampled_image";
				graphicsTests->addChild(new PushDescriptorImageGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorImageComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
				testName += "_sampler";
				graphicsTests->addChild(new PushDescriptorImageGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorImageComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				testName += "_storage_image";
				graphicsTests->addChild(new PushDescriptorImageGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorImageComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				testName += "_uniform_texel_buffer";
				graphicsTests->addChild(new PushDescriptorTexelBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorTexelBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				testName += "_storage_texel_buffer";
				graphicsTests->addChild(new PushDescriptorTexelBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
					computeTests->addChild(new PushDescriptorTexelBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				// Input attachments are not supported with dynamic rendering
				if (!vk::isConstructionTypeShaderObject(pipelineType))
				{
					testName += "_input_attachment";
					graphicsTests->addChild(new PushDescriptorInputAttachmentGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				}
				break;

			default:
				DE_FATAL("Unexpected descriptor type");
				break;
		}
	}

	if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		TestParams testParams = { pipelineType, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0u, 1u, true };
		graphicsTests->addChild(new PushDescriptorTexelBufferGraphicsTest(testCtx, "maintenance5_uniform_texel_buffer", "", testParams));
		testParams.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		graphicsTests->addChild(new PushDescriptorTexelBufferGraphicsTest(testCtx, "maintenance5_storage_texel_buffer", "", testParams));
		testParams.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		graphicsTests->addChild(new PushDescriptorBufferGraphicsTest(testCtx, "maintenance5_uniform_buffer", "", testParams));
	}

	pushDescriptorTests->addChild(graphicsTests.release());
	if (pipelineType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		pushDescriptorTests->addChild(computeTests.release());

	return pushDescriptorTests.release();
}

} // pipeline
} // vkt
