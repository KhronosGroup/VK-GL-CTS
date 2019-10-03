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
 * \brief Dynamic Offset Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDynamicOffsetTests.hpp"
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
#include "vkBuilderUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMemory.h"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace std;

namespace
{
typedef de::SharedPtr<Unique<VkBuffer> >		VkBufferSp;
typedef de::SharedPtr<Allocation>				AllocationSp;
typedef de::SharedPtr<Unique<VkCommandBuffer> >	VkCommandBufferSp;
typedef de::SharedPtr<Unique<VkRenderPass> >	VkRenderPassSp;
typedef de::SharedPtr<Unique<VkFramebuffer> >	VkFramebufferSp;
typedef de::SharedPtr<Unique<VkPipeline> >		VkPipelineSp;

struct TestParams
{
	VkDescriptorType	descriptorType;
	deUint32			numCmdBuffers;
	bool				reverseOrder;
	deUint32			numDescriptorSetBindings;
	deUint32			numDynamicBindings;
	deUint32			numNonDynamicBindings;
};

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

static const tcu::Vec4			testColors[]	=
{
	tcu::Vec4(0.3f, 0.0f, 0.0f, 1.0f),
	tcu::Vec4(0.0f, 0.3f, 0.0f, 1.0f),
	tcu::Vec4(0.0f, 0.0f, 0.3f, 1.0f),
	tcu::Vec4(0.3f, 0.3f, 0.0f, 1.0f),
	tcu::Vec4(0.0f, 0.3f, 0.3f, 1.0f),
	tcu::Vec4(0.3f, 0.0f, 0.3f, 1.0f)
};
static constexpr VkDeviceSize	kColorSize		= static_cast<VkDeviceSize>(sizeof(testColors[0]));
static constexpr deUint32		kNumTestColors	= static_cast<deUint32>(DE_LENGTH_OF_ARRAY(testColors));

class DynamicOffsetTestInstance : public vkt::TestInstance
{
public:
	DynamicOffsetTestInstance (Context& context, const TestParams& params)
		: vkt::TestInstance	(context)
		, m_params			(params)
		, m_memAlloc		(context.getDeviceInterface(), context.getDevice(), getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
		{}

protected:
	const TestParams	m_params;
	SimpleAllocator		m_memAlloc;
};

class DynamicOffsetGraphicsTestInstance : public DynamicOffsetTestInstance
{
public:
								DynamicOffsetGraphicsTestInstance	(Context& context, const TestParams& params);
	virtual						~DynamicOffsetGraphicsTestInstance	(void);
	void						init								(void);
	virtual tcu::TestStatus		iterate								(void);
	tcu::TestStatus				verifyImage							(void);

private:
	const tcu::UVec2			m_renderSize;
	const VkFormat				m_colorFormat;
	VkImageCreateInfo			m_colorImageCreateInfo;
	Move<VkImage>				m_colorImage;
	de::MovePtr<Allocation>		m_colorImageAlloc;
	Move<VkImageView>			m_colorAttachmentView;
	vector<VkRenderPassSp>		m_renderPasses;
	vector<VkFramebufferSp>		m_framebuffers;
	Move<VkShaderModule>		m_vertexShaderModule;
	Move<VkShaderModule>		m_fragmentShaderModule;
	Move<VkBuffer>				m_vertexBuffer;
	de::MovePtr<Allocation>		m_vertexBufferAlloc;
	Move<VkBuffer>				m_buffer;
	de::MovePtr<Allocation>		m_bufferAlloc;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSet>		m_descriptorSet;
	Move<VkPipelineLayout>		m_pipelineLayout;
	vector<VkPipelineSp>		m_graphicsPipelines;
	Move<VkCommandPool>			m_cmdPool;
	vector<VkCommandBufferSp>	m_cmdBuffers;
	vector<Vertex4RGBA>			m_vertices;
};

DynamicOffsetGraphicsTestInstance::DynamicOffsetGraphicsTestInstance (Context& context, const TestParams& params)
	: DynamicOffsetTestInstance	(context, params)
	, m_renderSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices				(createQuads(m_params.numDescriptorSetBindings * m_params.numCmdBuffers, 0.25f))
{
}

void DynamicOffsetGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA		= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const DeviceInterface&			vk							= m_context.getDeviceInterface();
	const VkDevice					vkDevice					= m_context.getDevice();
	const deUint32					queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const deUint32					numBindings					= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	deUint32						offset						= 0;
	deUint32						quadNdx						= 0;
	const VkPhysicalDeviceLimits	deviceLimits				= getPhysicalDeviceProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()).limits;
	const VkDeviceSize				colorBlockInputSize			= de::max(kColorSize, m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? deviceLimits.minUniformBufferOffsetAlignment : deviceLimits.minStorageBufferOffsetAlignment);
	const VkDeviceSize				bufferSize					= colorBlockInputSize * kNumTestColors;
	const VkDeviceSize				bindingOffset				= bufferSize / numBindings;
	const VkDescriptorType			nonDynamicDescriptorType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	// Create color image
	{

		const VkImageCreateInfo colorImageParams =
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
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout			initialLayout;
		};

		m_colorImageCreateInfo	= colorImageParams;
		m_colorImage			= createImage(vk, vkDevice, &m_colorImageCreateInfo);

		// Allocate and bind color image memory
		m_colorImageAlloc		= m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
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

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render passes
	for (deUint32 renderPassIdx = 0; renderPassIdx < 2; renderPassIdx++)
	{
		// The first pass clears the output image, and the second one draws on top of the first pass.
		const VkAttachmentLoadOp		loadOps[]				=
		{
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_LOAD_OP_LOAD
		};

		const VkImageLayout				initialLayouts[]		=
		{
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		const VkAttachmentDescription	attachmentDescription	=
		{
			(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
			m_colorFormat,								// VkFormat						format
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
			loadOps[renderPassIdx],						// VkAttachmentLoadOp			loadOp
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
			initialLayouts[renderPassIdx],				// VkImageLayout				initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
		};

		const VkAttachmentReference		attachmentRef			=
		{
			0u,											// deUint32			attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
		};

		const VkSubpassDescription		subpassDescription		=
		{
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint			pipelineBindPoint
			0u,									// deUint32						inputAttachmentCount
			DE_NULL,							// const VkAttachmentReference*	pInputAttachments
			1u,									// deUint32						colorAttachmentCount
			&attachmentRef,						// const VkAttachmentReference*	pColorAttachments
			DE_NULL,							// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,							// const VkAttachmentReference*	pDepthStencilAttachment
			0u,									// deUint32						preserveAttachmentCount
			DE_NULL								// const deUint32*				pPreserveAttachments
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

		m_renderPasses.push_back(VkRenderPassSp(new Unique<VkRenderPass>(createRenderPass(vk, vkDevice, &renderPassInfo))));
	}

	// Create framebuffers
	for (deUint32 framebufferIdx = 0; framebufferIdx < 2; framebufferIdx++)
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
			**m_renderPasses[framebufferIdx],			// VkRenderPass					renderPass;
			1u,											// deUint32						attachmentCount;
			attachmentBindInfos,						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32						width;
			(deUint32)m_renderSize.y(),					// deUint32						height;
			1u											// deUint32						layers;
		};

		m_framebuffers.push_back(VkFramebufferSp(new Unique<VkFramebuffer>(createFramebuffer(vk, vkDevice, &framebufferParams))));
	}

	// Create pipeline layout
	{
		// Create descriptor set layout
		vector<VkDescriptorSetLayoutBinding>	descriptorSetLayoutBindings;

		for (deUint32 binding = 0; binding < numBindings; binding++)
		{
			const VkDescriptorType					descriptorType					= binding >= m_params.numDynamicBindings ? nonDynamicDescriptorType : m_params.descriptorType;
			const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
			{
				binding,					// uint32_t				binding;
				descriptorType,				// VkDescriptorType		descriptorType;
				1u,							// uint32_t				descriptorCount;
				VK_SHADER_STAGE_VERTEX_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL						// const VkSampler*		pImmutableSamplers;
			};

			descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
		}

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkDescriptorSetLayoutCreateFlags		flags;
			numBindings,											// uint32_t								bindingCount;
			descriptorSetLayoutBindings.data()						// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL);

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

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create buffer
	{
		vector<deUint8> hostBuffer((size_t)bufferSize, 0);
		for (deUint32 colorIdx = 0; colorIdx < kNumTestColors; colorIdx++)
			deMemcpy(&hostBuffer[(deUint32)colorBlockInputSize * colorIdx], &testColors[colorIdx], kColorSize);

		const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			bufferSize,								// VkDeviceSize			size;
			usageFlags,								// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_buffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
		m_bufferAlloc = m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_buffer, m_bufferAlloc->getMemory(), m_bufferAlloc->getOffset()));

		deMemcpy(m_bufferAlloc->getHostPtr(), hostBuffer.data(), (size_t)bufferSize);
		flushAlloc(vk, vkDevice, *m_bufferAlloc);
	}

	// Create descriptor pool
	{
		DescriptorPoolBuilder	poolBuilder;
		poolBuilder.addType(m_params.descriptorType, m_params.numDynamicBindings);
		poolBuilder.addType(nonDynamicDescriptorType, m_params.numNonDynamicBindings);
		m_descriptorPool = poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	}

	// Create descriptor set
	{
		const VkDescriptorSetAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			*m_descriptorPool,								// VkDescriptorPool					descriptorPool;
			1u,												// deUint32							setLayoutCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*		pSetLayouts;
		};
		m_descriptorSet	= allocateDescriptorSet(vk, vkDevice, &allocInfo);
	}

	// Update descriptor set
	for (deUint32 binding = 0; binding < numBindings; ++binding)
	{
		const VkDescriptorType			descriptorType			= binding >= m_params.numDynamicBindings ? nonDynamicDescriptorType : m_params.descriptorType;
		const VkDescriptorBufferInfo	descriptorBufferInfo	=
		{
			*m_buffer,					// VkBuffer			buffer;
			bindingOffset * binding,	// VkDeviceSize		offset;
			kColorSize					// VkDeviceSize		range;
		};

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
			DE_NULL,								// const void*						pNext;
			*m_descriptorSet,						// VkDescriptorSet					dstSet;
			binding,								// uint32_t							dstBinding;
			0u,										// uint32_t							dstArrayElement;
			1u,										// uint32_t							descriptorCount;
			descriptorType,							// VkDescriptorType					descriptorType;
			DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
			&descriptorBufferInfo,					// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL									// const VkBufferView*				pTexelBufferView;
		};

		vk.updateDescriptorSets(vkDevice, 1u, &writeDescriptorSet, 0u, DE_NULL);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0u);
	}

	// Create pipelines
	for (deUint32 pipelineIdx = 0; pipelineIdx < 2; pipelineIdx++)
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

		m_graphicsPipelines.push_back(VkPipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(vk,								// const DeviceInterface&						vk
																							   vkDevice,						// const VkDevice								device
																							   *m_pipelineLayout,				// const VkPipelineLayout						pipelineLayout
																							   *m_vertexShaderModule,			// const VkShaderModule							vertexShaderModule
																							   DE_NULL,							// const VkShaderModule							tessellationControlShaderModule
																							   DE_NULL,							// const VkShaderModule							tessellationEvalShaderModule
																							   DE_NULL,							// const VkShaderModule							geometryShaderModule
																							   *m_fragmentShaderModule,			// const VkShaderModule							fragmentShaderModule
																							   **m_renderPasses[pipelineIdx],	// const VkRenderPass							renderPass
																							   viewports,						// const std::vector<VkViewport>&				viewports
																							   scissors,						// const std::vector<VkRect2D>&					scissors
																							   topology,						// const VkPrimitiveTopology					topology
																							   0u,								// const deUint32								subpass
																							   0u,								// const deUint32								patchControlPoints
																							   &vertexInputStateParams))));		// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
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
			&queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffers
	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
		m_cmdBuffers.push_back(VkCommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY))));

	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
	{
		const VkClearValue	attachmentClearValue	= defaultClearValue(m_colorFormat);
		const VkDeviceSize	vertexBufferOffset		= 0;
		const deUint32		idx						= m_params.reverseOrder ? m_params.numCmdBuffers - cmdBufferIdx - 1 : cmdBufferIdx;

		beginCommandBuffer(vk, **m_cmdBuffers[idx], 0u);
		beginRenderPass(vk, **m_cmdBuffers[idx], **m_renderPasses[idx], **m_framebuffers[idx], makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		vk.cmdBindPipeline(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, **m_graphicsPipelines[idx]);
		vk.cmdBindVertexBuffers(**m_cmdBuffers[idx], 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		for (deUint32 i = 0; i < m_params.numDescriptorSetBindings; i++)
		{
			vector<deUint32>	offsets;
			for (deUint32 dynamicBindingIdx = 0; dynamicBindingIdx < m_params.numDynamicBindings; dynamicBindingIdx++)
				offsets.push_back(offset + (deUint32)colorBlockInputSize * dynamicBindingIdx);

			vk.cmdBindDescriptorSets(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), m_params.numDynamicBindings, offsets.data());
			offset += (deUint32)colorBlockInputSize;

			// Draw quad
			vk.cmdDraw(**m_cmdBuffers[idx], 6, 1, 6 * quadNdx, 0);
			quadNdx++;
		}

		endRenderPass(vk, **m_cmdBuffers[idx]);
		endCommandBuffer(vk, **m_cmdBuffers[idx]);
	}
}

DynamicOffsetGraphicsTestInstance::~DynamicOffsetGraphicsTestInstance (void)
{
}

tcu::TestStatus DynamicOffsetGraphicsTestInstance::iterate (void)
{
	init();

	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
		submitCommandsAndWait(m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueue(), **m_cmdBuffers[cmdBufferIdx]);

	return verifyImage();
}

tcu::TestStatus DynamicOffsetGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat		= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat		= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader		(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program				(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer			(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk			= false;

	// Render reference image
	{
		const deUint32	numBindings		= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
		const deUint32	bindingOffset	= kNumTestColors / numBindings;

		for (deUint32 quadIdx = 0; quadIdx < m_vertices.size() / 6; quadIdx++)
			for (deUint32 vertexIdx = 0; vertexIdx < 6; vertexIdx++)
			{
				tcu::Vec4 refColor(0.0f);

				for (deUint32 binding = 0; binding < m_params.numDynamicBindings; binding++)
					refColor += testColors[quadIdx + binding * bindingOffset + binding];
				for (deUint32 binding = 0; binding < m_params.numNonDynamicBindings; binding++)
					refColor += testColors[(m_params.numDynamicBindings + binding) * bindingOffset];
				refColor.w() = 1.0f;

				m_vertices[quadIdx * 6 + vertexIdx].color.xyzw() = refColor;
			}

		refRenderer.draw(rr::RenderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits),
			             rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
	}

	// Compare result with reference image
	{
		de::MovePtr<tcu::TextureLevel> result = readColorAttachment(
			m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueue(),
			m_context.getUniversalQueueFamilyIndex(), m_memAlloc, *m_colorImage, m_colorFormat, m_renderSize);

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

class DynamicOffsetGraphicsTest : public vkt::TestCase
{
public:
						DynamicOffsetGraphicsTest	(tcu::TestContext&	testContext,
													 const string&		name,
													 const string&		description,
													 const TestParams&	params);
						~DynamicOffsetGraphicsTest	(void);
	void				initPrograms				(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance				(Context& context) const;

protected:
	const TestParams	m_params;
};

DynamicOffsetGraphicsTest::DynamicOffsetGraphicsTest (tcu::TestContext&	testContext,
													  const string&		name,
													  const string&		description,
													  const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

DynamicOffsetGraphicsTest::~DynamicOffsetGraphicsTest (void)
{
}

TestInstance* DynamicOffsetGraphicsTest::createInstance (Context& context) const
{
	return new DynamicOffsetGraphicsTestInstance(context, m_params);
}

void DynamicOffsetGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const deUint32	numBindings	= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	const string	bufferType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? "uniform" : "readonly buffer";
	string			inputBlocks;
	string			inputSum;

	for (deUint32 binding = 0; binding < numBindings; binding++)
	{
		const string b = de::toString(binding);
		inputBlocks +=
			string("layout(set = 0, binding = ") + b + ") " + bufferType + " Block" + b + "\n"
			+ "{\n" + "    vec4 color;\n" + "} inputData" + b + ";\n";
		inputSum += string("    vtxColor.rgb += inputData") + b + ".color.rgb;\n";
	}

	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		+ inputBlocks +
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"    gl_Position = position;\n"
		"    vtxColor = vec4(0, 0, 0, 1);\n"
		+ inputSum +
		"}\n";

	const string	fragmentSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    fragColor = vtxColor;\n"
		"}\n";

	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSrc);
	sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSrc);
}

class DynamicOffsetComputeTestInstance : public DynamicOffsetTestInstance
{
public:
								DynamicOffsetComputeTestInstance	(Context& context, const TestParams& params);
	virtual						~DynamicOffsetComputeTestInstance	(void);
	void						init								(void);
	virtual tcu::TestStatus		iterate								(void);
	tcu::TestStatus				verifyOutput						(void);

private:
	const deUint32					m_numBindings;
	const deUint32					m_numOutputColors;
	const VkPhysicalDeviceLimits	m_deviceLimits;
	Move<VkShaderModule>			m_computeShaderModule;
	Move<VkBuffer>					m_buffer;
	de::MovePtr<Allocation>			m_bufferAlloc;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_computePipeline;
	Move<VkBuffer>					m_outputBuffer;
	de::MovePtr<Allocation>			m_outputBufferAlloc;
	Move<VkCommandPool>				m_cmdPool;
	vector<VkCommandBufferSp>		m_cmdBuffers;
};

DynamicOffsetComputeTestInstance::DynamicOffsetComputeTestInstance (Context& context, const TestParams& params)
	: DynamicOffsetTestInstance	(context, params)
	, m_numBindings				(params.numDynamicBindings + params.numNonDynamicBindings)
	, m_numOutputColors			(params.numCmdBuffers * params.numDescriptorSetBindings)
	, m_deviceLimits			(getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits)
{
}

void DynamicOffsetComputeTestInstance::init (void)
{
	const DeviceInterface&		vk							= m_context.getDeviceInterface();
	const VkDevice				vkDevice					= m_context.getDevice();
	const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize			colorBlockInputSize			= de::max(kColorSize, m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? m_deviceLimits.minUniformBufferOffsetAlignment : m_deviceLimits.minStorageBufferOffsetAlignment);
	const deUint32				colorBlockInputSizeU32		= static_cast<deUint32>(colorBlockInputSize);
	const VkDeviceSize			colorBlockOutputSize		= de::max(kColorSize, m_deviceLimits.minStorageBufferOffsetAlignment);
	const deUint32				colorBlockOutputSizeU32		= static_cast<deUint32>(colorBlockOutputSize);
	const VkDeviceSize			bufferSize					= colorBlockInputSize * kNumTestColors;
	const VkDeviceSize			bindingOffset				= bufferSize / m_numBindings;
	const VkDescriptorType		nonDynamicDescriptorType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const VkDeviceSize			outputBufferSize			= colorBlockOutputSize * m_numOutputColors;

	// Create pipeline layout
	{
		// Create descriptor set layout
		vector<VkDescriptorSetLayoutBinding>	descriptorSetLayoutBindings;

		for (deUint32 binding = 0; binding < m_numBindings; binding++)
		{
			const VkDescriptorType					descriptorType				= binding >= m_params.numDynamicBindings ? nonDynamicDescriptorType : m_params.descriptorType;
			const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding	=
			{
				binding,						// uint32_t				binding;
				descriptorType,					// VkDescriptorType		descriptorType;
				1u,								// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL							// const VkSampler*		pImmutableSamplers;
			};

			descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
		}

		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingOutput	=
		{
			m_numBindings,								// uint32_t				binding;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	// VkDescriptorType		descriptorType;
			1u,											// uint32_t				descriptorCount;
			VK_SHADER_STAGE_COMPUTE_BIT,				// VkShaderStageFlags	stageFlags;
			DE_NULL										// const VkSampler*		pImmutableSamplers;
		};

		descriptorSetLayoutBindings.push_back(descriptorSetLayoutBindingOutput);

		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkDescriptorSetLayoutCreateFlags		flags;
			m_numBindings + 1,										// uint32_t								bindingCount;
			descriptorSetLayoutBindings.data()						// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL);

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

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create buffer
	{
		vector<deUint8> hostBuffer((deUint32)bufferSize, 0);
		for (deUint32 colorIdx = 0; colorIdx < kNumTestColors; colorIdx++)
			deMemcpy(&hostBuffer[colorBlockInputSizeU32 * colorIdx], &testColors[colorIdx], kColorSize);

		const VkBufferUsageFlags	usageFlags			= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			bufferSize,								// VkDeviceSize			size;
			usageFlags,								// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_buffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
		m_bufferAlloc = m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_buffer, m_bufferAlloc->getMemory(), m_bufferAlloc->getOffset()));

		deMemcpy(m_bufferAlloc->getHostPtr(), hostBuffer.data(), (size_t)bufferSize);
		flushAlloc(vk, vkDevice, *m_bufferAlloc);
	}

	// Create output buffer
	{
		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			outputBufferSize,						// VkDeviceSize			size;
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_outputBuffer		= createBuffer(vk, vkDevice, &bufferCreateInfo);
		m_outputBufferAlloc	= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_outputBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_outputBuffer, m_outputBufferAlloc->getMemory(), m_outputBufferAlloc->getOffset()));
	}

	// Create descriptor pool
	{
		DescriptorPoolBuilder	poolBuilder;
		poolBuilder.addType(m_params.descriptorType, m_params.numDynamicBindings);
		poolBuilder.addType(nonDynamicDescriptorType, m_params.numNonDynamicBindings);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1u);
		m_descriptorPool = poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	}

	// Create descriptor set
	{
		const VkDescriptorSetAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			*m_descriptorPool,								// VkDescriptorPool				descriptorPool;
			1u,												// deUint32						setLayoutCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
		};
		m_descriptorSet	= allocateDescriptorSet(vk, vkDevice, &allocInfo);
	}

	// Update input buffer descriptors
	for (deUint32 binding = 0; binding < m_numBindings; ++binding)
	{
		const VkDescriptorType			descriptorType			= binding >= m_params.numDynamicBindings ? nonDynamicDescriptorType : m_params.descriptorType;
		const VkDescriptorBufferInfo	descriptorBufferInfo	=
		{
			*m_buffer,					// VkBuffer			buffer;
			bindingOffset * binding,	// VkDeviceSize		offset;
			kColorSize					// VkDeviceSize		range;
		};

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
			DE_NULL,								// const void*						pNext;
			*m_descriptorSet,						// VkDescriptorSet					dstSet;
			binding,								// uint32_t							dstBinding;
			0u,										// uint32_t							dstArrayElement;
			1u,										// uint32_t							descriptorCount;
			descriptorType,							// VkDescriptorType					descriptorType;
			DE_NULL,								// const VkDescriptorImageInfo*		pImageInfo;
			&descriptorBufferInfo,					// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL									// const VkBufferView*				pTexelBufferView;
		};

		vk.updateDescriptorSets(vkDevice, 1u, &writeDescriptorSet, 0u, DE_NULL);
	}

	// Update output buffer descriptor
	{
		const VkDescriptorBufferInfo	descriptorBufferInfo	=
		{
			*m_outputBuffer,	// VkBuffer			buffer;
			0u,					// VkDeviceSize		offset;
			kColorSize			// VkDeviceSize		range;
		};

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			*m_descriptorSet,							// VkDescriptorSet					dstSet;
			m_numBindings,								// uint32_t							dstBinding;
			0u,											// uint32_t							dstArrayElement;
			1u,											// uint32_t							descriptorCount;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	// VkDescriptorType					descriptorType;
			DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo;
			&descriptorBufferInfo,						// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL										// const VkBufferView*				pTexelBufferView;
		};

		vk.updateDescriptorSets(vkDevice, 1u, &writeDescriptorSet, 0u, DE_NULL);
	}

	// Create shader
	{
		m_computeShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("compute"), 0u);
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
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineCreateFlags			flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo	stage;
			*m_pipelineLayout,										// VkPipelineLayout					layout;
			(VkPipeline)0,											// VkPipeline						basePipelineHandle;
			0u,														// int32_t							basePipelineIndex;
		};

		m_computePipeline = createComputePipeline(vk, vkDevice, (vk::VkPipelineCache)0u, &createInfo);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffers
	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
		m_cmdBuffers.push_back(VkCommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY))));

	deUint32 inputOffset	= 0u;
	deUint32 outputOffset	= 0u;

	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
	{
		const deUint32 idx = m_params.reverseOrder ? m_params.numCmdBuffers - cmdBufferIdx - 1 : cmdBufferIdx;

		beginCommandBuffer(vk, **m_cmdBuffers[idx], 0u);
		vk.cmdBindPipeline(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

		for (deUint32 i = 0; i < m_params.numDescriptorSetBindings; i++)
		{
			vector<deUint32> offsets;

			// Offsets for input buffers
			for (deUint32 dynamicBindingIdx = 0; dynamicBindingIdx < m_params.numDynamicBindings; dynamicBindingIdx++)
				offsets.push_back(inputOffset + colorBlockInputSizeU32 * dynamicBindingIdx);
			inputOffset += colorBlockInputSizeU32;

			// Offset for output buffer
			offsets.push_back(outputOffset);
			outputOffset += colorBlockOutputSizeU32;

			vk.cmdBindDescriptorSets(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), (deUint32)offsets.size(), offsets.data());

			// Dispatch
			vk.cmdDispatch(**m_cmdBuffers[idx], 1, 1, 1);
		}

		endCommandBuffer(vk, **m_cmdBuffers[idx]);
	}
}

DynamicOffsetComputeTestInstance::~DynamicOffsetComputeTestInstance (void)
{
}

tcu::TestStatus DynamicOffsetComputeTestInstance::iterate (void)
{
	init();

	for (deUint32 cmdBufferIdx = 0; cmdBufferIdx < m_params.numCmdBuffers; cmdBufferIdx++)
		submitCommandsAndWait(m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueue(), **m_cmdBuffers[cmdBufferIdx]);

	return verifyOutput();
}

tcu::TestStatus DynamicOffsetComputeTestInstance::verifyOutput (void)
{
	const deUint32		bindingOffset			= kNumTestColors / m_numBindings;
	const deUint32		colorBlockOutputSize	= static_cast<deUint32>(de::max(kColorSize, m_deviceLimits.minStorageBufferOffsetAlignment));
	vector<tcu::Vec4>	refColors				(m_numOutputColors);
	vector<tcu::Vec4>	outColors				(m_numOutputColors);

	for (deUint32 i = 0; i < m_numOutputColors; i++)
	{
		tcu::Vec4 refColor(0.0f);

		for (deUint32 binding = 0; binding < m_params.numDynamicBindings; binding++)
			refColor += testColors[i + binding * bindingOffset + binding];
		for (deUint32 binding = 0; binding < m_params.numNonDynamicBindings; binding++)
			refColor += testColors[(m_params.numDynamicBindings + binding) * bindingOffset];
		refColor.w() = 1.0f;

		refColors[i] = refColor;
	}

	invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_outputBufferAlloc);

	// Grab the output results using offset alignment
	for (deUint32 i = 0; i < m_numOutputColors; i++)
		outColors[i] = *(tcu::Vec4*)((deUint8*)m_outputBufferAlloc->getHostPtr() + colorBlockOutputSize * i);

	// Verify results
	for (deUint32 i = 0; i < m_numOutputColors; i++)
		if (outColors[i] != refColors[i])
			return tcu::TestStatus::fail("Output mismatch");

	return tcu::TestStatus::pass("Output matches expected values");
}

class DynamicOffsetComputeTest : public vkt::TestCase
{
public:
						DynamicOffsetComputeTest	(tcu::TestContext&	testContext,
													 const string&		name,
													 const string&		description,
													 const TestParams&	params);
						~DynamicOffsetComputeTest	(void);
	void				initPrograms				(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance				(Context& context) const;

protected:
	const TestParams	m_params;
};

DynamicOffsetComputeTest::DynamicOffsetComputeTest (tcu::TestContext&	testContext,
													const string&		name,
													const string&		description,
													const TestParams&	params)
	: vkt::TestCase	(testContext, name, description)
	, m_params		(params)
{
}

DynamicOffsetComputeTest::~DynamicOffsetComputeTest (void)
{
}

TestInstance* DynamicOffsetComputeTest::createInstance (Context& context) const
{
	return new DynamicOffsetComputeTestInstance(context, m_params);
}

void DynamicOffsetComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	const deUint32	numBindings	= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	const string	bufferType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? "uniform" : "buffer";
	string			inputBlocks;
	string			inputSum;

	for (deUint32 binding = 0; binding < numBindings; binding++)
	{
		const string b = de::toString(binding);
		inputBlocks +=
			string("layout(set = 0, binding = ") + b + ") " + bufferType + " Block" + b + "\n"
			+ "{\n" + "    vec4 color;\n" + "} inputData" + b + ";\n";
		inputSum += string("    outData.color.rgb += inputData") + b + ".color.rgb;\n";
	}

	const string	computeSrc	=
		"#version 450\n"
		+ inputBlocks +
		"layout(set = 0, binding = " + de::toString(numBindings) + ") writeonly buffer Output\n"
		"{\n"
		"	vec4 color;\n"
		"} outData;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    outData.color = vec4(0, 0, 0, 1);\n"
		+ inputSum +
		"}\n";

	sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
}

} // anonymous

tcu::TestCaseGroup* createDynamicOffsetTests (tcu::TestContext& testCtx)
{
	const char*	pipelineTypes[]			= { "graphics", "compute" };

	struct
	{
		const char*			name;
		VkDescriptorType	type;
	}
	const descriptorTypes[]				=
	{
		{	"uniform_buffer",	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC	},
		{	"storage_buffer",	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC	}
	};

	struct
	{
		const char*		name;
		deUint32		num;
	}
	const numCmdBuffers[]				=
	{
		{	"numcmdbuffers_1",	1u	},
		{	"numcmdbuffers_2",	2u	}
	};

	struct
	{
		const char*		name;
		bool			reverse;
	}
	const reverseOrders[]				=
	{
		{	"reverseorder",	true	},
		{	"sameorder",	false	}
	};

	struct
	{
		const char*		name;
		deUint32		num;
	}
	const numDescriptorSetBindings[]	=
	{
		{	"numdescriptorsetbindings_1",	1u	},
		{	"numdescriptorsetbindings_2",	2u	}
	};

	struct
	{
		const char*		name;
		deUint32		num;
	}
	const numDynamicBindings[]			=
	{
		{	"numdynamicbindings_1",	1u	},
		{	"numdynamicbindings_2",	2u	}
	};

	struct
	{
		const char*		name;
		deUint32		num;
	}
	const numNonDynamicBindings[]		=
	{
		{	"numnondynamicbindings_0",	0u	},
		{	"numnondynamicbindings_1",	1u	}
	};

	de::MovePtr<tcu::TestCaseGroup>	dynamicOffsetTests	(new tcu::TestCaseGroup(testCtx, "dynamic_offset", "Dynamic offset tests"));
	de::MovePtr<tcu::TestCaseGroup>	graphicsTests		(new tcu::TestCaseGroup(testCtx, "graphics", "graphics pipeline"));

	for (deUint32 pipelineTypeIdx = 0; pipelineTypeIdx < DE_LENGTH_OF_ARRAY(pipelineTypes); pipelineTypeIdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	pipelineTypeGroup	(new tcu::TestCaseGroup(testCtx, pipelineTypes[pipelineTypeIdx], ""));

		for (deUint32 descriptorTypeIdx = 0; descriptorTypeIdx < DE_LENGTH_OF_ARRAY(descriptorTypes); descriptorTypeIdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	descriptorTypeGroup	(new tcu::TestCaseGroup(testCtx, descriptorTypes[descriptorTypeIdx].name, ""));

			for (deUint32 numCmdBuffersIdx = 0; numCmdBuffersIdx < DE_LENGTH_OF_ARRAY(numCmdBuffers); numCmdBuffersIdx++)
			{
				de::MovePtr<tcu::TestCaseGroup>	numCmdBuffersGroup	(new tcu::TestCaseGroup(testCtx, numCmdBuffers[numCmdBuffersIdx].name, ""));

				for (deUint32 reverseOrderIdx = 0; reverseOrderIdx < DE_LENGTH_OF_ARRAY(reverseOrders); reverseOrderIdx++)
				{
					if (numCmdBuffers[numCmdBuffersIdx].num < 2 && reverseOrders[reverseOrderIdx].reverse)
						continue;

					de::MovePtr<tcu::TestCaseGroup>	reverseOrderGroup	(new tcu::TestCaseGroup(testCtx, reverseOrders[reverseOrderIdx].name, ""));

					for (deUint32 numDescriptorSetBindingsIdx = 0; numDescriptorSetBindingsIdx < DE_LENGTH_OF_ARRAY(numDescriptorSetBindings); numDescriptorSetBindingsIdx++)
					{
						if (numCmdBuffers[numCmdBuffersIdx].num > 1 && numDescriptorSetBindings[numDescriptorSetBindingsIdx].num > 1)
							continue;

						de::MovePtr<tcu::TestCaseGroup>	numDescriptorSetBindingsGroup	(new tcu::TestCaseGroup(testCtx, numDescriptorSetBindings[numDescriptorSetBindingsIdx].name, ""));
						for (deUint32 numDynamicBindingsIdx = 0; numDynamicBindingsIdx < DE_LENGTH_OF_ARRAY(numDynamicBindings); numDynamicBindingsIdx++)
						{
							de::MovePtr<tcu::TestCaseGroup>	numDynamicBindingsGroup	(new tcu::TestCaseGroup(testCtx, numDynamicBindings[numDynamicBindingsIdx].name, ""));

							for (deUint32 numNonDynamicBindingsIdx = 0; numNonDynamicBindingsIdx < DE_LENGTH_OF_ARRAY(numNonDynamicBindings); numNonDynamicBindingsIdx++)
							{
								TestParams params;
								params.descriptorType			= descriptorTypes[descriptorTypeIdx].type;
								params.numCmdBuffers			= numCmdBuffers[numCmdBuffersIdx].num;
								params.reverseOrder				= reverseOrders[reverseOrderIdx].reverse;
								params.numDescriptorSetBindings	= numDescriptorSetBindings[numDescriptorSetBindingsIdx].num;
								params.numDynamicBindings		= numDynamicBindings[numDynamicBindingsIdx].num;
								params.numNonDynamicBindings	= numNonDynamicBindings[numNonDynamicBindingsIdx].num;

								if (strcmp(pipelineTypes[pipelineTypeIdx], "graphics") == 0)
									numDynamicBindingsGroup->addChild(new DynamicOffsetGraphicsTest(testCtx, numNonDynamicBindings[numNonDynamicBindingsIdx].name, "", params));
								else
									numDynamicBindingsGroup->addChild(new DynamicOffsetComputeTest(testCtx, numNonDynamicBindings[numNonDynamicBindingsIdx].name, "", params));
							}

							numDescriptorSetBindingsGroup->addChild(numDynamicBindingsGroup.release());
						}

						reverseOrderGroup->addChild(numDescriptorSetBindingsGroup.release());
					}

					numCmdBuffersGroup->addChild(reverseOrderGroup.release());
				}

				descriptorTypeGroup->addChild(numCmdBuffersGroup.release());
			}

			pipelineTypeGroup->addChild(descriptorTypeGroup.release());
		}
		dynamicOffsetTests->addChild(pipelineTypeGroup.release());
	}

	return dynamicOffsetTests.release();
}

} // pipeline
} // vkt
