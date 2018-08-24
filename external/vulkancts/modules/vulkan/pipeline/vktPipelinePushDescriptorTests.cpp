/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2018 ARM Limited.
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
#include <vector>

// TODO: Currently only uniform and storage buffer descriptor types are tested.
//
// Tests for the following descriptor types are still missing:
// - VK_DESCRIPTOR_TYPE_SAMPLER
// - VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
// - VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
// - VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
// - VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
// - VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
// - VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace std;

namespace
{
typedef vector<VkExtensionProperties>		Extensions;
typedef de::SharedPtr<Unique<VkBuffer> >	VkBufferSp;
typedef de::SharedPtr<Allocation>			AllocationSp;

struct TestParams
{
	VkDescriptorType	descriptorType;
	deUint32			binding;
	deUint32			numCalls; // Number of draw or dispatch calls
};

void checkAllSupported (const Extensions& supportedExtensions, const vector<string>& requiredExtensions)
{
	for (vector<string>::const_iterator requiredExtName = requiredExtensions.begin(); requiredExtName != requiredExtensions.end(); ++requiredExtName)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(*requiredExtName)))
			TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
	}
}

Move<VkInstance> createInstanceWithGetPhysicalDeviceProperties2 (const PlatformInterface&	vkp,
																 deUint32					version,
																 const Extensions&			supportedExtensions)
{
	vector<string> extensions;

	if (!isCoreInstanceExtension(version, "VK_KHR_get_physical_device_properties2"))
		extensions.push_back("VK_KHR_get_physical_device_properties2");

	checkAllSupported(supportedExtensions, extensions);

	return createDefaultInstance(vkp, version, vector<string>(), extensions);
}

Move<VkDevice> createDeviceWithPushDescriptor (const PlatformInterface&		vkp,
											   VkInstance					instance,
											   const InstanceInterface&		vki,
											   VkPhysicalDevice				physicalDevice,
											   const Extensions&			supportedExtensions,
											   const deUint32				queueFamilyIndex)
{

	const float						queuePriority	= 1.0f;
	const VkDeviceQueueCreateInfo	queueInfo		=
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

	const char* const				extensions[]	=
	{
		"VK_KHR_push_descriptor"
	};

	const VkDeviceCreateInfo		deviceParams    =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		DE_NULL,
		(VkDeviceCreateFlags)0,
		1u,
		&queueInfo,
		0u,
		DE_NULL,
		1u,
		extensions,
		&features
	};

	if (!isExtensionSupported(supportedExtensions, RequiredExtension(extensions[0])))
		TCU_THROW(NotSupportedError, (string(extensions[0]) + " is not supported").c_str());

	return createDevice(vkp, instance, vki, physicalDevice, &deviceParams, DE_NULL);
}

deUint32 findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkQueueFamilyProperties>	queueProps	= getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
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

static const tcu::Vec4 testColors[] =
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
	const TestParams			m_params;
	const PlatformInterface&	m_vkp;
	const Extensions			m_instanceExtensions;
	const Unique<VkInstance>	m_instance;
	const InstanceDriver		m_vki;
	const VkPhysicalDevice		m_physicalDevice;
	const deUint32				m_queueFamilyIndex;
	const Extensions			m_deviceExtensions;
	const Unique<VkDevice>		m_device;
	const DeviceDriver			m_vkd;
	const VkQueue				m_queue;
	SimpleAllocator				m_allocator;
	const tcu::UVec2			m_renderSize;
	const VkFormat				m_colorFormat;
	VkImageCreateInfo			m_colorImageCreateInfo;
	Move<VkImage>				m_colorImage;
	de::MovePtr<Allocation>		m_colorImageAlloc;
	Move<VkImageView>			m_colorAttachmentView;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkShaderModule>		m_vertexShaderModule;
	Move<VkShaderModule>		m_fragmentShaderModule;
	Move<VkBuffer>				m_vertexBuffer;
	de::MovePtr<Allocation>		m_vertexBufferAlloc;
	vector<VkBufferSp>			m_buffers;
	vector<AllocationSp>		m_bufferAllocs;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_graphicsPipelines;
	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
	vector<Vertex4RGBA>			m_vertices;
};

PushDescriptorBufferGraphicsTestInstance::PushDescriptorBufferGraphicsTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance		(context)
	, m_params				(params)
	, m_vkp					(context.getPlatformInterface())
	, m_instanceExtensions	(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance			(createInstanceWithGetPhysicalDeviceProperties2(m_vkp, context.getUsedApiVersion(), m_instanceExtensions))
	, m_vki					(m_vkp, *m_instance)
	, m_physicalDevice		(chooseDevice(m_vki, *m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT))
	, m_deviceExtensions	(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device				(createDeviceWithPushDescriptor(m_vkp, *m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex))
	, m_vkd					(m_vkp, *m_instance, *m_device)
	, m_queue				(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
	, m_allocator			(m_vkd, *m_device, getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_renderSize			(32, 32)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices			(createQuads(params.numCalls, 0.25f))
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

		m_colorImageCreateInfo	= colorImageParams;
		m_colorImage			= createImage(m_vkd, *m_device, &m_colorImageCreateInfo);

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
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange		subresourceRange;
		};

		m_colorAttachmentView = createImageView(m_vkd, *m_device, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = makeRenderPass(m_vkd, *m_device, m_colorFormat);

	// Create framebuffer
	{
		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkFramebufferCreateFlags		flags;
			*m_renderPass,								// VkRenderPass					renderPass;
			1u,											// deUint32						attachmentCount;
			attachmentBindInfos,						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32						width;
			(deUint32)m_renderSize.y(),					// deUint32						height;
			1u											// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(m_vkd, *m_device, &framebufferParams);
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
		{
			m_params.binding,					// uint32_t              binding;
			m_params.descriptorType,			// VkDescriptorType      descriptorType;
			1u,									// uint32_t              descriptorCount;
			VK_SHADER_STAGE_VERTEX_BIT,			// VkShaderStageFlags    stageFlags;
			DE_NULL								// const VkSampler*      pImmutableSamplers;
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType                        sType;
			DE_NULL,													// const void*                            pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags       flags;
			1u,															// uint32_t                               bindingCount;
			&descriptorSetLayoutBinding									// const VkDescriptorSetLayoutBinding*    pBindings;
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

	// Create buffers. One color value in each buffer.
	{
		for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(testColors); bufIdx++)
		{
			const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			const VkBufferCreateInfo	bufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u,										// VkBufferCreateFlags	flags
				16u,									// VkDeviceSize			size;
				usageFlags,								// VkBufferUsageFlags	usage;
				VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
				1u,										// deUint32				queueFamilyCount;
				&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
			};

			m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
			m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
			VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

			deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &testColors[bufIdx], 16u);
			flushMappedMemoryRange(m_vkd, *m_device, m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset(), 16u);
		}
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(m_vkd, *m_device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= createShaderModule(m_vkd, *m_device, m_context.getBinaryCollection().get("frag"), 0u);
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

		const vector<VkViewport>					viewports							(1, makeViewport(m_renderSize));
		const vector<VkRect2D>						scissors							(1, makeRect2D(m_renderSize));

		m_graphicsPipelines = makeGraphicsPipeline(m_vkd,						// const DeviceInterface&                        vk
												   *m_device,					// const VkDevice                                device
												   *m_pipelineLayout,			// const VkPipelineLayout                        pipelineLayout
												   *m_vertexShaderModule,		// const VkShaderModule                          vertexShaderModule
												   DE_NULL,						// const VkShaderModule                          tessellationControlShaderModule
												   DE_NULL,						// const VkShaderModule                          tessellationEvalShaderModule
												   DE_NULL,						// const VkShaderModule                          geometryShaderModule
												   *m_fragmentShaderModule,		// const VkShaderModule                          fragmentShaderModule
												   *m_renderPass,				// const VkRenderPass                            renderPass
												   viewports,					// const std::vector<VkViewport>&                viewports
												   scissors,					// const std::vector<VkRect2D>&                  scissors
												   topology,					// const VkPrimitiveTopology                     topology
												   0u,							// const deUint32                                subpass
												   0u,							// const deUint32                                patchControlPoints
												   &vertexInputStateParams);	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
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
		flushMappedMemoryRange(m_vkd, *m_device, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	m_cmdPool = createCommandPool(m_vkd, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;

		m_cmdBuffer = allocateCommandBuffer(m_vkd, *m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(m_vkd, *m_cmdBuffer, 0u);
		beginRenderPass(m_vkd, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		m_vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);
		m_vkd.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		// Draw quads. Switch input buffer which contains the quad color for each draw call.
		for (deUint32 quadNdx = 0; quadNdx < m_params.numCalls; quadNdx++)
		{
			VkDescriptorBufferInfo descriptorBufferInfo =
			{
				**m_buffers[quadNdx],	// VkBuffer        buffer;
				0u,						// VkDeviceSize    offset;
				16u						// VkDeviceSize    range;
			};

			VkWriteDescriptorSet writeDescriptorSet =
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
				DE_NULL,								// const void*                      pNext;
				0u,										// VkDescriptorSet                  dstSet;
				m_params.binding,						// uint32_t                         dstBinding;
				0u,										// uint32_t                         dstArrayElement;
				1u,										// uint32_t                         descriptorCount;
				m_params.descriptorType,				// VkDescriptorType                 descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*     pImageInfo;
				&descriptorBufferInfo,					// const VkDescriptorBufferInfo*    pBufferInfo;
				DE_NULL									// const VkBufferView*              pTexelBufferView;
			};

			m_vkd.cmdPushDescriptorSetKHR(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &writeDescriptorSet);
			m_vkd.cmdDraw(*m_cmdBuffer, 6, 1, 6 * quadNdx, 0);
		}

		endRenderPass(m_vkd, *m_cmdBuffer);
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
				m_vertices[quadIdx * 6 + vertexIdx].color.xyzw() = testColors[quadIdx];

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState()), rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
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

void PushDescriptorBufferGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const string	bufferType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? "uniform" : "buffer";
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
	const Unique<VkInstance>	m_instance;
	const InstanceDriver		m_vki;
	const VkPhysicalDevice		m_physicalDevice;
	const deUint32				m_queueFamilyIndex;
	const Extensions			m_deviceExtensions;
	const Unique<VkDevice>		m_device;
	const DeviceDriver			m_vkd;
	const VkQueue				m_queue;
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
};

PushDescriptorBufferComputeTestInstance::PushDescriptorBufferComputeTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance		(context)
	, m_params				(params)
	, m_vkp					(context.getPlatformInterface())
	, m_instanceExtensions	(enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
	, m_instance			(createInstanceWithGetPhysicalDeviceProperties2(m_vkp, context.getUsedApiVersion(), m_instanceExtensions))
	, m_vki					(m_vkp, *m_instance)
	, m_physicalDevice		(chooseDevice(m_vki, *m_instance, context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(findQueueFamilyIndexWithCaps(m_vki, m_physicalDevice, VK_QUEUE_COMPUTE_BIT))
	, m_deviceExtensions	(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device				(createDeviceWithPushDescriptor(m_vkp, *m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex))
	, m_vkd					(m_vkp, *m_instance, *m_device)
	, m_queue				(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))
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
				m_params.binding,				// uint32_t              binding;
				m_params.descriptorType,		// VkDescriptorType      descriptorType;
				1u,								// uint32_t              descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags    stageFlags;
				DE_NULL							// const VkSampler*      pImmutableSamplers;
			},
			{
				m_params.binding + 1,				// uint32_t              binding;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// VkDescriptorType      descriptorType;
				1u,									// uint32_t              descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags    stageFlags;
				DE_NULL								// const VkSampler*      pImmutableSamplers;
			}
		};

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType                        sType;
			DE_NULL,													// const void*                            pNext;
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,	// VkDescriptorSetLayoutCreateFlags       flags;
			2u,															// uint32_t                               bindingCount;
			descriptorSetLayoutBindings									// const VkDescriptorSetLayoutBinding*    pBindings;
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

	// Create buffers. One color value in each buffer.
	{
		for (deUint32 bufIdx = 0; bufIdx < DE_LENGTH_OF_ARRAY(testColors); bufIdx++)
		{
			const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			const VkBufferCreateInfo	bufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u,										// VkBufferCreateFlags	flags
				16u,									// VkDeviceSize			size;
				usageFlags,								// VkBufferUsageFlags	usage;
				VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
				1u,										// deUint32				queueFamilyCount;
				&m_queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
			};

			m_buffers.push_back(VkBufferSp(new Unique<VkBuffer>(createBuffer(m_vkd, *m_device, &bufferCreateInfo))));
			m_bufferAllocs.push_back(AllocationSp(m_allocator.allocate(getBufferMemoryRequirements(m_vkd, *m_device, **m_buffers[bufIdx]), MemoryRequirement::HostVisible).release()));
			VK_CHECK(m_vkd.bindBufferMemory(*m_device, **m_buffers[bufIdx], m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset()));

			deMemcpy(m_bufferAllocs[bufIdx]->getHostPtr(), &testColors[bufIdx], 16u);
			flushMappedMemoryRange(m_vkd, *m_device, m_bufferAllocs[bufIdx]->getMemory(), m_bufferAllocs[bufIdx]->getOffset(), 16u);
		}
	}

	// Create output buffer
	{
		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			32u,									// VkDeviceSize			size;
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
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*m_computeShaderModule,									// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		createInfo		=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType                    sType;
			DE_NULL,												// const void*                        pNext;
			0u,														// VkPipelineCreateFlags              flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo    stage;
			*m_pipelineLayout,										// VkPipelineLayout                   layout;
			(VkPipeline)0,											// VkPipeline                         basePipelineHandle;
			0u,														// int32_t                            basePipelineIndex;
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
				**m_buffers[dispatchNdx],	// VkBuffer        buffer;
				0u,							// VkDeviceSize    offset;
				16u							// VkDeviceSize    range;
			};

			VkDescriptorBufferInfo descriptorBufferInfoOutput	=
			{
				*m_outputBuffer,	// VkBuffer        buffer;
				16u * dispatchNdx,	// VkDeviceSize    offset;
				16u					// VkDeviceSize    range;
			};

			VkWriteDescriptorSet writeDescriptorSets[] =
			{
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
					DE_NULL,								// const void*                      pNext;
					0u,										// VkDescriptorSet                  dstSet;
					m_params.binding,						// uint32_t                         dstBinding;
					0u,										// uint32_t                         dstArrayElement;
					1u,										// uint32_t                         descriptorCount;
					m_params.descriptorType,				// VkDescriptorType                 descriptorType;
					DE_NULL,								// const VkDescriptorImageInfo*     pImageInfo;
					&descriptorBufferInfoUbo,				// const VkDescriptorBufferInfo*    pBufferInfo;
					DE_NULL									// const VkBufferView*              pTexelBufferView;
				},
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
					DE_NULL,								// const void*                      pNext;
					0u,										// VkDescriptorSet                  dstSet;
					m_params.binding + 1,					// uint32_t                         dstBinding;
					0u,										// uint32_t                         dstArrayElement;
					1u,										// uint32_t                         descriptorCount;
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType                 descriptorType;
					DE_NULL,								// const VkDescriptorImageInfo*     pImageInfo;
					&descriptorBufferInfoOutput,			// const VkDescriptorBufferInfo*    pBufferInfo;
					DE_NULL									// const VkBufferView*              pTexelBufferView;
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
	invalidateMappedMemoryRange(m_vkd, *m_device, m_outputBufferAlloc->getMemory(), m_outputBufferAlloc->getOffset(), (size_t)32u);

	// Verify result
	if (deMemCmp((void*)testColors, m_outputBufferAlloc->getHostPtr(), (size_t)(16u * m_params.numCalls)))
	{
		return tcu::TestStatus::fail("Output mismatch");
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

} // anonymous

tcu::TestCaseGroup* createPushDescriptorTests (tcu::TestContext& testCtx)
{
	const TestParams params[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 1u },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 2u },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, 2u },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3u, 2u },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0u, 1u },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0u, 2u },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, 2u },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u, 2u },
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
				graphicsTests->addChild(new PushDescriptorBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				computeTests->addChild(new PushDescriptorBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				testName += "_storage_buffer";
				graphicsTests->addChild(new PushDescriptorBufferGraphicsTest(testCtx, testName.c_str(), "", params[testIdx]));
				computeTests->addChild(new PushDescriptorBufferComputeTest(testCtx, testName.c_str(), "", params[testIdx]));
				break;

			default:
				DE_FATAL("Unexpected descriptor type");
				break;
		};
	}

	pushDescriptorTests->addChild(graphicsTests.release());
	pushDescriptorTests->addChild(computeTests.release());

	return pushDescriptorTests.release();
}

} // pipeline
} // vkt
