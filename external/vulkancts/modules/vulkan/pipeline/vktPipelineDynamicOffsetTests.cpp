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
 * \brief Dynamic Offset Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDynamicOffsetTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vkComputePipelineConstructionUtil.hpp"
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
#include <array>
#include <cmath>
#include <vector>
#include <sstream>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace std;
using de::UniquePtr;

namespace
{
typedef de::SharedPtr<Unique<VkBuffer>>			VkBufferSp;
typedef de::SharedPtr<Allocation>				AllocationSp;
typedef de::SharedPtr<Unique<VkCommandBuffer>>	VkCommandBufferSp;
typedef de::SharedPtr<RenderPassWrapper>		VkRenderPassSp;

enum class GroupingStrategy
{
	SINGLE_SET	= 0,
	MULTISET	= 1,
	ARRAYS		= 2,
};

struct TestParams
{
	PipelineConstructionType	pipelineConstructionType;
	VkDescriptorType			descriptorType;
	deUint32					numCmdBuffers;
	bool						reverseOrder;
	deUint32					numDescriptorSetBindings;
	deUint32					numDynamicBindings;
	deUint32					numNonDynamicBindings;
	GroupingStrategy			groupingStrategy;
};
#ifndef CTS_USES_VULKANSC
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
#endif // CTS_USES_VULKANSC

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

bool compareVectors (const tcu::Vec4 firstVector, const tcu::Vec4 secondVector, const float tolerance)
{
	for (auto i = 0; i < firstVector.SIZE; i++)
	{
		if (abs(firstVector[i] - secondVector[i]) > tolerance)
			return false;
	}

	return true;
}

inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	return imageParams;
}

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
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	VkImageCreateInfo					m_colorImageCreateInfo;
	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	vector<VkRenderPassSp>				m_renderPasses;
	ShaderWrapper						m_vertexShaderModule;
	ShaderWrapper						m_fragmentShaderModule;
	Move<VkBuffer>						m_vertexBuffer;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;
	Move<VkBuffer>						m_buffer;
	de::MovePtr<Allocation>				m_bufferAlloc;
	vector<Move<VkDescriptorSetLayout>>	m_descriptorSetLayouts;
	Move<VkDescriptorPool>				m_descriptorPool;
	vector<Move<VkDescriptorSet>>		m_descriptorSets;
	PipelineLayoutWrapper				m_pipelineLayout;
	vector<GraphicsPipelineWrapper>		m_graphicsPipelines;
	Move<VkCommandPool>					m_cmdPool;
	vector<VkCommandBufferSp>			m_cmdBuffers;
	vector<Vertex4RGBA>					m_vertices;
};
#ifndef CTS_USES_VULKANSC
DynamicOffsetGraphicsTestInstance::DynamicOffsetGraphicsTestInstance (Context& context, const TestParams& params)
	: DynamicOffsetTestInstance	(context, params)
	, m_renderSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices				(createQuads(m_params.numDescriptorSetBindings * m_params.numCmdBuffers, 0.25f))
{
}
#endif // CTS_USES_VULKANSC

void DynamicOffsetGraphicsTestInstance::init (void)
{
	const VkComponentMapping		componentMappingRGBA		= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const InstanceInterface&		vki							= m_context.getInstanceInterface();
	const DeviceInterface&			vk							= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice				= m_context.getPhysicalDevice();
	const VkDevice					vkDevice					= m_context.getDevice();
	const deUint32					queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const deUint32					numBindings					= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	deUint32						offset						= 0;
	deUint32						quadNdx						= 0;
	const VkPhysicalDeviceLimits	deviceLimits				= getPhysicalDeviceProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()).limits;
	const VkDeviceSize				alignment					= ((m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ? deviceLimits.minUniformBufferOffsetAlignment : deviceLimits.minStorageBufferOffsetAlignment);
	const VkDeviceSize				extraBytes					= kColorSize % alignment;
	const VkDeviceSize				colorBlockInputSize			= ((extraBytes == 0ull) ? kColorSize : (kColorSize + alignment - extraBytes));
	const VkDeviceSize				bufferSize					= colorBlockInputSize * kNumTestColors;
	const VkDeviceSize				bindingOffset				= bufferSize / numBindings;
	const VkDescriptorType			nonDynamicDescriptorType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	vector<VkDescriptorSetLayout>	descriptorSetLayoutsPlain;
	vector<VkDescriptorSet>			descriptorSetsPlain;

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
	for (deUint32 renderPassIdx = 0; renderPassIdx < m_params.numCmdBuffers; renderPassIdx++)
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

		m_renderPasses.push_back(VkRenderPassSp(new RenderPassWrapper(m_params.pipelineConstructionType, vk, vkDevice, &renderPassInfo)));

		const VkImageView				attachmentBindInfos[]	=
		{
			*m_colorAttachmentView
		};

		const VkFramebufferCreateInfo	framebufferParams		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkFramebufferCreateFlags		flags;
			**m_renderPasses[renderPassIdx],			// VkRenderPass					renderPass;
			1u,											// deUint32						attachmentCount;
			attachmentBindInfos,						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32						width;
			(deUint32)m_renderSize.y(),					// deUint32						height;
			1u											// deUint32						layers;
		};

		m_renderPasses[renderPassIdx]->createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		// Create descriptor set layouts
		vector<VkDescriptorSetLayoutBinding>	descriptorSetLayoutBindings;

		for (deUint32 binding = 0; binding < numBindings; binding++)
		{
			const bool								dynamicDesc						= (binding < m_params.numDynamicBindings);
			const VkDescriptorType					descriptorType					= (dynamicDesc ? m_params.descriptorType : nonDynamicDescriptorType);
			const deUint32							bindingNumber					= (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET ? binding : 0u);
			const deUint32							descriptorCount					= ((m_params.groupingStrategy == GroupingStrategy::ARRAYS) ? (dynamicDesc ? m_params.numDynamicBindings : m_params.numNonDynamicBindings) : 1u);
			const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding		=
			{
				bindingNumber,				// uint32_t				binding;
				descriptorType,				// VkDescriptorType		descriptorType;
				descriptorCount,			// uint32_t				descriptorCount;
				VK_SHADER_STAGE_VERTEX_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL						// const VkSampler*		pImmutableSamplers;
			};

			// Skip used descriptors in array mode.
			if (m_params.groupingStrategy == GroupingStrategy::ARRAYS)
				binding = (dynamicDesc ? m_params.numDynamicBindings - 1 : numBindings);

			descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
		}

		vector<VkDescriptorSetLayoutCreateInfo> descriptorSetLayoutCreateInfos;

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				0u,														// VkDescriptorSetLayoutCreateFlags		flags;
				numBindings,											// uint32_t								bindingCount;
				descriptorSetLayoutBindings.data()						// const VkDescriptorSetLayoutBinding*	pBindings;
			};

			m_descriptorSetLayouts.push_back(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo));
		}
		else
		{
			for (size_t i = 0; i < descriptorSetLayoutBindings.size(); ++i)
			{
				const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
				{
					VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
					DE_NULL,												// const void*							pNext;
					0u,														// VkDescriptorSetLayoutCreateFlags		flags;
					1u,														// uint32_t								bindingCount;
					&descriptorSetLayoutBindings[i]							// const VkDescriptorSetLayoutBinding*	pBindings;
				};

				m_descriptorSetLayouts.push_back(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo));
			}
		}

		// Create pipeline layout
		descriptorSetLayoutsPlain.resize(m_descriptorSetLayouts.size());
		for (size_t i = 0; i < descriptorSetLayoutsPlain.size(); ++i)
			descriptorSetLayoutsPlain[i] = m_descriptorSetLayouts[i].get();

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// VkStructureType				sType;
			DE_NULL,													// const void*					pNext;
			0u,															// VkPipelineLayoutCreateFlags	flags;
			static_cast<deUint32>(descriptorSetLayoutsPlain.size()),	// deUint32						descriptorSetCount;
			descriptorSetLayoutsPlain.data(),							// const VkDescriptorSetLayout*	pSetLayouts;
			0u,															// deUint32						pushConstantRangeCount;
			DE_NULL														// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
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
		m_descriptorPool = poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, static_cast<deUint32>(m_descriptorSetLayouts.size()));
	}

	// Create descriptor sets
	{
		for (size_t i = 0; i < m_descriptorSetLayouts.size(); ++i)
		{
			const VkDescriptorSetAllocateInfo allocInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType					sType;
				DE_NULL,										// const void*						pNext;
				*m_descriptorPool,								// VkDescriptorPool					descriptorPool;
				1u,												// deUint32							setLayoutCount;
				&(m_descriptorSetLayouts[i].get()),				// const VkDescriptorSetLayout*		pSetLayouts;
			};
			m_descriptorSets.push_back(allocateDescriptorSet(vk, vkDevice, &allocInfo));
		}
	}

	descriptorSetsPlain.resize(m_descriptorSets.size());
	for (size_t i = 0; i < descriptorSetsPlain.size(); ++i)
		descriptorSetsPlain[i] = m_descriptorSets[i].get();

	// Update descriptor sets
	for (deUint32 binding = 0; binding < numBindings; ++binding)
	{
		const bool						dynamicDesc				= (binding < m_params.numDynamicBindings);
		const VkDescriptorType			descriptorType			= (dynamicDesc ? m_params.descriptorType : nonDynamicDescriptorType);
		const VkDescriptorBufferInfo	descriptorBufferInfo	=
		{
			*m_buffer,					// VkBuffer			buffer;
			bindingOffset * binding,	// VkDeviceSize		offset;
			kColorSize					// VkDeviceSize		range;
		};

		VkDescriptorSet	bindingSet;
		deUint32		bindingNumber;
		deUint32		dstArrayElement;

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			bindingSet		= m_descriptorSets[0].get();
			bindingNumber	= binding;
			dstArrayElement	= 0u;
		}
		else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
		{
			bindingSet		= m_descriptorSets[binding].get();
			bindingNumber	= 0u;
			dstArrayElement	= 0u;
		}
		else // GroupingStrategy::ARRAYS
		{
			bindingSet		= (dynamicDesc ? m_descriptorSets[0].get() : m_descriptorSets[1].get());
			bindingNumber	= 0u;
			dstArrayElement	= (dynamicDesc ? binding : (binding - m_params.numDynamicBindings));
		}

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
			DE_NULL,								// const void*						pNext;
			bindingSet,								// VkDescriptorSet					dstSet;
			bindingNumber,							// uint32_t							dstBinding;
			dstArrayElement,						// uint32_t							dstArrayElement;
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
		m_vertexShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0u);
	}

	// Create pipelines
	m_graphicsPipelines.reserve(m_params.numCmdBuffers);
	for (deUint32 pipelineIdx = 0; pipelineIdx < m_params.numCmdBuffers; pipelineIdx++)
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

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams
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

		m_graphicsPipelines.emplace_back(vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
		m_graphicsPipelines.back().setMonolithicPipelineLayout(m_pipelineLayout)
								  .setDefaultRasterizationState()
								  .setDefaultDepthStencilState()
								  .setDefaultColorBlendState()
								  .setDefaultMultisampleState()
								  .setupVertexInputState(&vertexInputStateParams)
								  .setupPreRasterizationShaderState(viewports,
																	scissors,
																	m_pipelineLayout,
																	**m_renderPasses[pipelineIdx],
																	0u,
																	m_vertexShaderModule)
								  .setupFragmentShaderState(m_pipelineLayout, **m_renderPasses[pipelineIdx], 0u, m_fragmentShaderModule)
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
		m_renderPasses[idx]->begin(vk, **m_cmdBuffers[idx], makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);
		m_graphicsPipelines[idx].bind(**m_cmdBuffers[idx]);
		vk.cmdBindVertexBuffers(**m_cmdBuffers[idx], 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

		for (deUint32 i = 0; i < m_params.numDescriptorSetBindings; i++)
		{
			vector<deUint32>	offsets;
			for (deUint32 dynamicBindingIdx = 0; dynamicBindingIdx < m_params.numDynamicBindings; dynamicBindingIdx++)
				offsets.push_back(offset + (deUint32)colorBlockInputSize * dynamicBindingIdx);

			vk.cmdBindDescriptorSets(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, static_cast<deUint32>(descriptorSetsPlain.size()), descriptorSetsPlain.data(), m_params.numDynamicBindings, offsets.data());
			offset += (deUint32)colorBlockInputSize;

			// Draw quad
			vk.cmdDraw(**m_cmdBuffers[idx], 6, 1, 6 * quadNdx, 0);
			quadNdx++;
		}

		m_renderPasses[idx]->end(vk, **m_cmdBuffers[idx]);
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
#ifndef CTS_USES_VULKANSC
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
	void				checkSupport				(Context& context) const;

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

void DynamicOffsetGraphicsTest::checkSupport(Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void DynamicOffsetGraphicsTest::initPrograms (SourceCollections& sourceCollections) const
{
	const deUint32	numBindings		= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	const string	bufferType		= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? "uniform" : "readonly buffer";
	ostringstream	inputBlocks;
	ostringstream	inputSum;
	string			setAndBinding;
	string			blockSuffix;
	string			accessSuffix;
	bool			dynArrayDecl	= false;	// Dynamic descriptor block array declared?
	bool			nonDynArrayDecl	= false;	// Nondynamic descriptor block array declared?

	for (deUint32 b = 0; b < numBindings; b++)
	{
		const bool		dynBind	= (b < m_params.numDynamicBindings);
		const string	bStr	= de::toString(b);

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			setAndBinding	= "set = 0, binding = " + bStr;
			blockSuffix		= bStr;
			accessSuffix	= bStr;
		}
		else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
		{
			setAndBinding	= "set = " + bStr + ", binding = 0";
			blockSuffix		= bStr;
			accessSuffix	= bStr;
		}
		else // GroupingStrategy::ARRAYS
		{
			// In array mode, only two sets are declared, one with an array of dynamic descriptors and another one with an array of
			// nondynamic descriptors.
			setAndBinding	= "set = " + string(dynBind ? "0" : "1") + ", binding = 0";
			blockSuffix		= string(dynBind ? "Dyn" : "NonDyn") + "[" + (dynBind ? de::toString(m_params.numDynamicBindings) : de::toString(m_params.numNonDynamicBindings)) + "]";
			accessSuffix	= string(dynBind ? "Dyn" : "NonDyn") + "[" + (dynBind ? de::toString(b) : de::toString(b - m_params.numDynamicBindings)) + "]";
		}

		// In array mode, declare the input block only once per descriptor type.
		bool& arrayDeclFlag = (dynBind ? dynArrayDecl : nonDynArrayDecl);
		if (m_params.groupingStrategy != GroupingStrategy::ARRAYS || !arrayDeclFlag)
		{
			inputBlocks
				<< "layout(" << setAndBinding << ") " << bufferType << " Block" << bStr << "\n"
				<< "{\n"
				<< "    vec4 color;\n"
				<< "} inputData" << blockSuffix << ";\n"
				;
			arrayDeclFlag = true;
		}

		// But the sum always needs to be added once per descriptor.
		inputSum << "    vtxColor.rgb += inputData" << accessSuffix << ".color.rgb;\n";
	}

	const string	vertexSrc	=
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		+ inputBlocks.str() +
		"\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main()\n"
		"{\n"
		"    gl_Position = position;\n"
		"    vtxColor = vec4(0, 0, 0, 1);\n"
		+ inputSum.str() +
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
#endif // CTS_USES_VULKANSC
class DynamicOffsetComputeTestInstance : public DynamicOffsetTestInstance
{
public:
								DynamicOffsetComputeTestInstance	(Context& context, const TestParams& params);
	virtual						~DynamicOffsetComputeTestInstance	(void);
	void						init								(void);
	virtual tcu::TestStatus		iterate								(void);
	tcu::TestStatus				verifyOutput						(void);

private:
	const deUint32						m_numBindings;
	const deUint32						m_numOutputColors;
	const VkPhysicalDeviceLimits		m_deviceLimits;
	Move<VkBuffer>						m_buffer;
	de::MovePtr<Allocation>				m_bufferAlloc;
	vector<Move<VkDescriptorSetLayout>>	m_descriptorSetLayouts;
	Move<VkDescriptorPool>				m_descriptorPool;
	vector<Move<VkDescriptorSet>>		m_descriptorSets;
	PipelineLayoutWrapper				m_pipelineLayout;
	ComputePipelineWrapper				m_computePipeline;
	Move<VkBuffer>						m_outputBuffer;
	de::MovePtr<Allocation>				m_outputBufferAlloc;
	Move<VkCommandPool>					m_cmdPool;
	vector<VkCommandBufferSp>			m_cmdBuffers;
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
	const DeviceInterface&			vk							= m_context.getDeviceInterface();
	const VkDevice					vkDevice					= m_context.getDevice();
	const deUint32					queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize				inputAlignment				= ((m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ? m_deviceLimits.minUniformBufferOffsetAlignment : m_deviceLimits.minStorageBufferOffsetAlignment);
	const VkDeviceSize				inputExtraBytes				= kColorSize % inputAlignment;
	const VkDeviceSize				colorBlockInputSize			= ((inputExtraBytes == 0ull) ? kColorSize : (kColorSize + inputAlignment - inputExtraBytes));
	const deUint32					colorBlockInputSizeU32		= static_cast<deUint32>(colorBlockInputSize);
	const VkDeviceSize				outputExtraBytes			= kColorSize % m_deviceLimits.minStorageBufferOffsetAlignment;
	const VkDeviceSize				colorBlockOutputSize		= ((outputExtraBytes == 0ull) ? kColorSize : (kColorSize + m_deviceLimits.minStorageBufferOffsetAlignment - outputExtraBytes));
	const deUint32					colorBlockOutputSizeU32		= static_cast<deUint32>(colorBlockOutputSize);
	const VkDeviceSize				bufferSize					= colorBlockInputSize * kNumTestColors;
	const VkDeviceSize				bindingOffset				= bufferSize / m_numBindings;
	const VkDescriptorType			nonDynamicDescriptorType	= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const VkDeviceSize				outputBufferSize			= colorBlockOutputSize * m_numOutputColors;

	vector<VkDescriptorSetLayout>	descriptorSetLayoutsPlain;
	vector<VkDescriptorSet>			descriptorSetsPlain;

	// Create pipeline layout
	{
		// Create descriptor set layouts
		vector<VkDescriptorSetLayoutBinding>	descriptorSetLayoutBindings;

		for (deUint32 binding = 0; binding < m_numBindings; binding++)
		{
			const bool								dynamicDesc					= (binding < m_params.numDynamicBindings);
			const VkDescriptorType					descriptorType				= (dynamicDesc ? m_params.descriptorType : nonDynamicDescriptorType);
			const deUint32							bindingNumber				= (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET ? binding : 0u);
			const deUint32							descriptorCount				= ((m_params.groupingStrategy == GroupingStrategy::ARRAYS) ? (dynamicDesc ? m_params.numDynamicBindings : m_params.numNonDynamicBindings) : 1u);
			const VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding	=
			{
				bindingNumber,					// uint32_t				binding;
				descriptorType,					// VkDescriptorType		descriptorType;
				descriptorCount,				// uint32_t				descriptorCount;
				VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
				DE_NULL							// const VkSampler*		pImmutableSamplers;
			};

			// Skip used descriptors in array mode.
			if (m_params.groupingStrategy == GroupingStrategy::ARRAYS)
				binding = (dynamicDesc ? m_params.numDynamicBindings - 1 : m_numBindings);

			descriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
		}

		const deUint32							bindingNumberOutput					= (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET ? m_numBindings : 0u);
		const VkDescriptorSetLayoutBinding		descriptorSetLayoutBindingOutput	=
		{
			bindingNumberOutput,						// uint32_t				binding;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	// VkDescriptorType		descriptorType;
			1u,											// uint32_t				descriptorCount;
			VK_SHADER_STAGE_COMPUTE_BIT,				// VkShaderStageFlags	stageFlags;
			DE_NULL										// const VkSampler*		pImmutableSamplers;
		};

		descriptorSetLayoutBindings.push_back(descriptorSetLayoutBindingOutput);

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				0u,														// VkDescriptorSetLayoutCreateFlags		flags;
				m_numBindings + 1,										// uint32_t								bindingCount;
				descriptorSetLayoutBindings.data()						// const VkDescriptorSetLayoutBinding*	pBindings;
			};

			m_descriptorSetLayouts.push_back(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL));
		}
		else
		{
			for (size_t i = 0; i < descriptorSetLayoutBindings.size(); ++i)
			{
				const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
				{
					VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
					DE_NULL,												// const void*							pNext;
					0u,														// VkDescriptorSetLayoutCreateFlags		flags;
					1u,														// uint32_t								bindingCount;
					&descriptorSetLayoutBindings[i]							// const VkDescriptorSetLayoutBinding*	pBindings;
				};

				m_descriptorSetLayouts.push_back(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutCreateInfo, DE_NULL));
			}
		}

		// Create pipeline layout
		descriptorSetLayoutsPlain.resize(m_descriptorSetLayouts.size());
		for (size_t i = 0; i < descriptorSetLayoutsPlain.size(); ++i)
			descriptorSetLayoutsPlain[i] = m_descriptorSetLayouts[i].get();

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// VkStructureType				sType;
			DE_NULL,													// const void*					pNext;
			0u,															// VkPipelineLayoutCreateFlags	flags;
			static_cast<deUint32>(descriptorSetLayoutsPlain.size()),	// deUint32						descriptorSetCount;
			descriptorSetLayoutsPlain.data(),							// const VkDescriptorSetLayout*	pSetLayouts;
			0u,															// deUint32						pushConstantRangeCount;
			DE_NULL														// const VkPushDescriptorRange*	pPushDescriptorRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
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
		m_descriptorPool = poolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, static_cast<deUint32>(m_descriptorSetLayouts.size()));
	}

	// Create descriptor sets
	{
		for (size_t i = 0; i < m_descriptorSetLayouts.size(); ++i)
		{
			const VkDescriptorSetAllocateInfo allocInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				*m_descriptorPool,								// VkDescriptorPool				descriptorPool;
				1u,												// deUint32						setLayoutCount;
				&(m_descriptorSetLayouts[i].get()),				// const VkDescriptorSetLayout*	pSetLayouts;
			};
			m_descriptorSets.push_back(allocateDescriptorSet(vk, vkDevice, &allocInfo));
		}
	}

	descriptorSetsPlain.resize(m_descriptorSets.size());
	for (size_t i = 0; i < descriptorSetsPlain.size(); ++i)
		descriptorSetsPlain[i] = m_descriptorSets[i].get();

	// Update input buffer descriptors
	for (deUint32 binding = 0; binding < m_numBindings; ++binding)
	{
		const bool						dynamicDesc				= (binding < m_params.numDynamicBindings);
		const VkDescriptorType			descriptorType			= dynamicDesc ? m_params.descriptorType : nonDynamicDescriptorType;
		const VkDescriptorBufferInfo	descriptorBufferInfo	=
		{
			*m_buffer,					// VkBuffer			buffer;
			bindingOffset * binding,	// VkDeviceSize		offset;
			kColorSize					// VkDeviceSize		range;
		};

		VkDescriptorSet	bindingSet;
		deUint32		bindingNumber;
		deUint32		dstArrayElement;

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			bindingSet		= m_descriptorSets[0].get();
			bindingNumber	= binding;
			dstArrayElement	= 0u;
		}
		else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
		{
			bindingSet		= m_descriptorSets[binding].get();
			bindingNumber	= 0u;
			dstArrayElement	= 0u;
		}
		else // GroupingStrategy::ARRAYS
		{
			bindingSet		= (dynamicDesc ? m_descriptorSets[0].get() : m_descriptorSets[1].get());
			bindingNumber	= 0u;
			dstArrayElement	= (dynamicDesc ? binding : (binding - m_params.numDynamicBindings));
		}

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType;
			DE_NULL,								// const void*						pNext;
			bindingSet,								// VkDescriptorSet					dstSet;
			bindingNumber,							// uint32_t							dstBinding;
			dstArrayElement,						// uint32_t							dstArrayElement;
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

		VkDescriptorSet	bindingSet;
		deUint32		bindingNumber;

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			bindingSet		= m_descriptorSets[0].get();
			bindingNumber	= m_numBindings;
		}
		else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
		{
			bindingSet		= m_descriptorSets.back().get();
			bindingNumber	= 0u;
		}
		else // GroupingStrategy::ARRAYS
		{
			bindingSet		= m_descriptorSets.back().get();
			bindingNumber	= 0u;
		}

		const VkWriteDescriptorSet		writeDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			bindingSet,									// VkDescriptorSet					dstSet;
			bindingNumber,								// uint32_t							dstBinding;
			0u,											// uint32_t							dstArrayElement;
			1u,											// uint32_t							descriptorCount;
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	// VkDescriptorType					descriptorType;
			DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo;
			&descriptorBufferInfo,						// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL										// const VkBufferView*				pTexelBufferView;
		};

		vk.updateDescriptorSets(vkDevice, 1u, &writeDescriptorSet, 0u, DE_NULL);
	}

	// Create pipeline
	{

		m_computePipeline = ComputePipelineWrapper(vk, vkDevice, graphicsToComputeConstructionType(m_params.pipelineConstructionType), m_context.getBinaryCollection().get("compute"));
		m_computePipeline.setDescriptorSetLayouts(m_pipelineLayout.getSetLayoutCount(), m_pipelineLayout.getSetLayouts());
		m_computePipeline.buildPipeline();
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
		m_computePipeline.bind(**m_cmdBuffers[idx]);

		for (deUint32 i = 0; i < m_params.numDescriptorSetBindings; i++)
		{
			// Create pipeline barrier
			const vk::VkBufferMemoryBarrier bufferBarrier =
				{
					vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
					DE_NULL,														// const void*		pNext;
					vk::VK_ACCESS_SHADER_WRITE_BIT,									// VkAccessFlags	srcAccessMask;
					vk::VK_ACCESS_SHADER_WRITE_BIT | vk::VK_ACCESS_HOST_READ_BIT,	// VkAccessFlags	dstAccessMask;
					VK_QUEUE_FAMILY_IGNORED,										// deUint32			srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,										// deUint32			dstQueueFamilyIndex;
					*m_outputBuffer,												// VkBuffer			buffer;
					outputOffset,													// VkDeviceSize		offset;
					VK_WHOLE_SIZE													// VkDeviceSize		size;
				};

			vector<deUint32> offsets;

			// Offsets for input buffers
			for (deUint32 dynamicBindingIdx = 0; dynamicBindingIdx < m_params.numDynamicBindings; dynamicBindingIdx++)
				offsets.push_back(inputOffset + colorBlockInputSizeU32 * dynamicBindingIdx);
			inputOffset += colorBlockInputSizeU32;

			// Offset for output buffer
			offsets.push_back(outputOffset);
			outputOffset += colorBlockOutputSizeU32;

			vk.cmdBindDescriptorSets(**m_cmdBuffers[idx], VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, static_cast<deUint32>(descriptorSetsPlain.size()), descriptorSetsPlain.data(), (deUint32)offsets.size(), offsets.data());

			// Dispatch
			vk.cmdDispatch(**m_cmdBuffers[idx], 1, 1, 1);

			vk.cmdPipelineBarrier(**m_cmdBuffers[idx], vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
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
	void				checkSupport				(Context& context) const;

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

void DynamicOffsetComputeTest::checkSupport(Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void DynamicOffsetComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	const deUint32	numBindings		= m_params.numDynamicBindings + m_params.numNonDynamicBindings;
	const string	bufferType		= m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ? "uniform" : "buffer";
	ostringstream	inputBlocks;
	ostringstream	inputSum;
	string			setAndBinding;
	string			blockSuffix;
	string			accessSuffix;
	bool			dynArrayDecl	= false;	// Dynamic descriptor block array declared?
	bool			nonDynArrayDecl	= false;	// Nondynamic descriptor block array declared?
	string			bStr;

	for (deUint32 b = 0; b < numBindings; b++)
	{
		const bool dynBind	= (b < m_params.numDynamicBindings);
		bStr				= de::toString(b);

		if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
		{
			setAndBinding = "set = 0, binding = " + bStr;
			blockSuffix		= bStr;
			accessSuffix	= bStr;
		}
		else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
		{
			setAndBinding = "set = " + bStr + ", binding = 0";
			blockSuffix		= bStr;
			accessSuffix	= bStr;
		}
		else // GroupingStrategy::ARRAYS
		{
			// In array mode, only two sets are declared, one with an array of dynamic descriptors and another one with an array of
			// nondynamic descriptors.
			setAndBinding	= "set = " + string(dynBind ? "0" : "1") + ", binding = 0";
			blockSuffix		= string(dynBind ? "Dyn" : "NonDyn") + "[" + (dynBind ? de::toString(m_params.numDynamicBindings) : de::toString(m_params.numNonDynamicBindings)) + "]";
			accessSuffix	= string(dynBind ? "Dyn" : "NonDyn") + "[" + (dynBind ? de::toString(b) : de::toString(b - m_params.numDynamicBindings)) + "]";
		}

		// In array mode, declare the input block only once per descriptor type.
		bool& arrayDeclFlag = (dynBind ? dynArrayDecl : nonDynArrayDecl);
		if (m_params.groupingStrategy != GroupingStrategy::ARRAYS || !arrayDeclFlag)
		{
			inputBlocks
				<< "layout(" << setAndBinding << ") " << bufferType << " Block" << bStr << "\n"
				<< "{\n"
				<< "    vec4 color;\n"
				<< "} inputData" << blockSuffix << ";\n"
				;
			arrayDeclFlag = true;
		}

		// But the sum always needs to be added once per descriptor.
		inputSum << "    outData.color.rgb += inputData" << accessSuffix << ".color.rgb;\n";
	}

	bStr = de::toString(numBindings);
	if (m_params.groupingStrategy == GroupingStrategy::SINGLE_SET)
	{
		setAndBinding = "set = 0, binding = " + bStr;
	}
	else if (m_params.groupingStrategy == GroupingStrategy::MULTISET)
	{
		setAndBinding = "set = " + bStr + ", binding = 0";
	}
	else // GroupingStrategy::ARRAYS
	{
		// The output buffer goes to a separate set.
		deUint32 usedSets = 0u;
		if (dynArrayDecl)		++usedSets;
		if (nonDynArrayDecl)	++usedSets;

		setAndBinding = "set = " + de::toString(usedSets) + ", binding = 0";
	}

	const string	computeSrc	=
		"#version 450\n"
		+ inputBlocks.str() +
		"layout(" + setAndBinding + ") writeonly buffer Output\n"
		"{\n"
		"	vec4 color;\n"
		"} outData;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    outData.color = vec4(0, 0, 0, 1);\n"
		+ inputSum.str() +
		"}\n";

	sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc);
}

class DynamicOffsetMixedTestInstance : public vkt::TestInstance
{
public:
								DynamicOffsetMixedTestInstance	(Context&						context,
																 const PipelineConstructionType	pipelineConstructionType,
																 const tcu::IVec2				renderSize,
																 const deUint32					numInstances,
																 const bool						testAllOffsets,
																 const bool						reverseOrder,
																 const bool						runComputeFirst,
																 const deUint32					vertexOffset,
																 const deUint32					sharedUboOffset,
																 const deUint32					fragUboOffset,
																 const deUint32					ssboReadOffset,
																 const deUint32					ssboWriteOffset)
																: vkt::TestInstance	(context)
																, m_pipelineConstructionType	(pipelineConstructionType)
																, m_renderSize					(renderSize)
																, m_numInstances				(numInstances)
																, m_testAllOffsets				(testAllOffsets)
																, m_reverseOrder				(reverseOrder)
																, m_runComputeFirst				(runComputeFirst)
																, m_vertexOffset				(vertexOffset)
																, m_sharedUboOffset				(sharedUboOffset)
																, m_fragUboOffset				(fragUboOffset)
																, m_ssboReadOffset				(ssboReadOffset)
																, m_ssboWriteOffset				(ssboWriteOffset)
																{}

								~DynamicOffsetMixedTestInstance	();

	virtual tcu::TestStatus		iterate							(void);

private:
	struct VertexInfo
	{
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	const VkFormat					OUTPUT_COLOR_FORMAT		= VK_FORMAT_R8G8B8A8_UNORM;

	const PipelineConstructionType	m_pipelineConstructionType;
	const tcu::IVec2				m_renderSize;
	const deUint32					m_numInstances;
	const bool						m_testAllOffsets;
	const bool						m_reverseOrder;
	const bool						m_runComputeFirst;
	const deUint32					m_vertexOffset;
	const deUint32					m_sharedUboOffset;
	const deUint32					m_fragUboOffset;
	const deUint32					m_ssboReadOffset;
	const deUint32					m_ssboWriteOffset;
};

DynamicOffsetMixedTestInstance::~DynamicOffsetMixedTestInstance ()
{
}

tcu::TestStatus DynamicOffsetMixedTestInstance::iterate (void)
{
	tcu::TestLog&											log						= m_context.getTestContext().getLog();
	const InstanceInterface&								vki						= m_context.getInstanceInterface();
	const DeviceInterface&									vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice									physicalDevice			= m_context.getPhysicalDevice();
	const VkDevice											device					= m_context.getDevice();
	Allocator&												allocator				= m_context.getDefaultAllocator();
	const deUint32											queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	// Create shaders
	const ShaderWrapper										vertexShaderModule		= ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper										fragmentShaderModule	= ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	const ShaderWrapper										computeShaderModule		= ShaderWrapper(vk, device, m_context.getBinaryCollection().get("comp"), 0u);

	const deUint32											vertexBufferBindId		= 0u;

	// Vertex input state and binding
	VkVertexInputBindingDescription							bindingDescription
	{
		vertexBufferBindId,			// uint32_t				binding;
		sizeof(VertexInfo),			// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate	inputRate;
	};

	const std::array<VkVertexInputAttributeDescription, 2>	vertexAttributeDescs
	{ {
		VkVertexInputAttributeDescription
		{
			0u,								// uint32_t	location;
			vertexBufferBindId,				// uint32_t	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
			0u								// uint32_t	offset;
		},

		VkVertexInputAttributeDescription
		{
			1u,								// uint32_t	location;
			vertexBufferBindId,				// uint32_t	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
			deUint32(sizeof(float)) * 4u	// uint32_t	offset;
		}
	} };

	const VkPipelineVertexInputStateCreateInfo				vertexInputStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineVertexInputStateCreateFlags	flags;
		1u,															// uint32_t									vertexBindingDescriptionCount;
		&bindingDescription,										// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		static_cast<uint32_t>(vertexAttributeDescs.size()),			// uint32_t									vertexAttributeDescriptionCount;
		vertexAttributeDescs.data()									// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Descriptor pool and descriptor set
	DescriptorPoolBuilder									poolBuilder;

	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3u);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2u);

	const Move<VkDescriptorPool>							descriptorPool			= poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	DescriptorSetLayoutBuilder								layoutBuilderAttachments;
	{
		if (!m_reverseOrder)
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		else
		{
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
			layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);
		}
	}

	const Move<VkDescriptorSetLayout>						descriptorSetLayout		= layoutBuilderAttachments.build(vk, device);

	const Move<VkDescriptorSet>								descriptorSet			= makeDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayout.get());

	Move<VkImage>											colorImage				= (makeImage(vk, device, makeImageCreateInfo(m_renderSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));

	// Allocate and bind color image memory
	const VkImageSubresourceRange							colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const UniquePtr<Allocation>								colorImageAlloc			(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	Move<VkImageView>										colorImageView			= (makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, OUTPUT_COLOR_FORMAT, colorSubresourceRange));

	// Create renderpass
	const VkAttachmentDescription							attachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags
		OUTPUT_COLOR_FORMAT,						// VkFormat						format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
	};

	const VkAttachmentReference								attachmentReference		=
	{
		0u,											// deUint32			attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
	};

	const VkSubpassDescription								subpassDescription		=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags	flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint			pipelineBindPoint
		0u,									// deUint32						inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*	pInputAttachments
		1u,									// deUint32						colorAttachmentCount
		&attachmentReference,				// const VkAttachmentReference*	pColorAttachments
		DE_NULL,							// const VkAttachmentReference*	pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*	pDepthStencilAttachment
		0u,									// deUint32						preserveAttachmentCount
		DE_NULL								// const deUint32*				pPreserveAttachments
	};

	const VkRenderPassCreateInfo							renderPassInfo			=
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

	RenderPassWrapper										renderPass				(m_pipelineConstructionType, vk, device, &renderPassInfo);

	// Create framebuffer
	const VkImageView										attachmentBindInfos[]	=
	{
		*colorImageView
	};

	const VkFramebufferCreateInfo							framebufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		VkFramebufferCreateFlags(0),				// VkFramebufferCreateFlags		flags;
		*renderPass,								// VkRenderPass					renderPass;
		1u,											// deUint32						attachmentCount;
		attachmentBindInfos,						// const VkImageView*			pAttachments;
		(deUint32)m_renderSize.x(),					// deUint32						width;
		(deUint32)m_renderSize.y(),					// deUint32						height;
		1u											// deUint32						layers;
	};

	renderPass.createFramebuffer(vk, device, &framebufferCreateInfo, *colorImage);

	// Create pipeline layout
	const VkPipelineLayoutCreateInfo						pipelineLayoutInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		0u,														// VkPipelineLayoutCreateFlags	flags;
		1u,														// deUint32						descriptorSetCount;
		&descriptorSetLayout.get(),								// const VkDescriptorSetLayout*	pSetLayouts;
		0u,														// deUint32						pushConstantRangeCount;
		DE_NULL													// const VkPushDescriptorRange*	pPushDescriptorRanges;
	};

	PipelineLayoutWrapper									pipelineLayout			(m_pipelineConstructionType, vk, device, &pipelineLayoutInfo);

	// Create graphics pipeline
	const std::vector<VkViewport>							viewports(1, makeViewport(m_renderSize));
	const std::vector<VkRect2D>								scissors(1, makeRect2D(m_renderSize));

	const VkPipelineRasterizationStateCreateInfo	rasterizationState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,													// const void*                                pNext
		0u,															// VkPipelineRasterizationStateCreateFlags    flags
		VK_FALSE,													// VkBool32                                   depthClampEnable
		VK_FALSE,													// VkBool32                                   rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags                            cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace                                frontFace
		VK_FALSE,													// VkBool32                                   depthBiasEnable
		0.0f,														// float                                      depthBiasConstantFactor
		0.0f,														// float                                      depthBiasClamp
		0.0f,														// float                                      depthBiasSlopeFactor
		1.0f														// float                                      lineWidth
	};

	GraphicsPipelineWrapper										graphicsPipeline		(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_pipelineConstructionType);

	graphicsPipeline.setDefaultMultisampleState()
		.setDefaultColorBlendState()
		.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setupVertexInputState(&vertexInputStateCreateInfo)
		.setupPreRasterizationShaderState((viewports),
			scissors,
			pipelineLayout,
			*renderPass,
			0u,
			vertexShaderModule,
			&rasterizationState)
		.setupFragmentShaderState(pipelineLayout,
			*renderPass,
			0u,
			fragmentShaderModule)
		.setupFragmentOutputState(*renderPass, 0u)
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();

	ComputePipelineWrapper									computePipeline			(vk, device, graphicsToComputeConstructionType(m_pipelineConstructionType), m_context.getBinaryCollection().get("comp"));
	computePipeline.setDescriptorSetLayout(descriptorSetLayout.get());
	computePipeline.buildPipeline();

	const VkQueue											queue					= m_context.getUniversalQueue();
	const VkPhysicalDeviceLimits							deviceLimits			= getPhysicalDeviceProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()).limits;

	// Create vertex buffer
	const deUint32											numVertices				= 6;
	const VkDeviceSize										vertexBufferSizeBytes	= 256;
	const Unique<VkBuffer>									vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const de::UniquePtr<Allocation>							vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	const deUint32											instanceSize			= (deUint32)std::sqrt(m_numInstances);
	const float												posIncrement			= 1.0f / (float)m_numInstances * float(instanceSize);

	// Result image has to be a square and multiple of 16.
	DE_ASSERT(instanceSize * instanceSize == m_numInstances && m_numInstances % 16u == 0);

	{
		tcu::Vec4			vertexColor	= tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f);
		VertexInfo*	const	pVertices	= reinterpret_cast<VertexInfo*>(vertexBufferAlloc->getHostPtr());

		pVertices[0]	= { tcu::Vec4(posIncrement, -posIncrement, 0.0f, 1.0f),		vertexColor };
		pVertices[1]	= { tcu::Vec4(-posIncrement, -posIncrement, 0.0f, 1.0f),	vertexColor };
		pVertices[2]	= { tcu::Vec4(-posIncrement, posIncrement, 0.0f, 1.0f),		vertexColor };
		pVertices[3]	= { tcu::Vec4(-posIncrement, posIncrement, 1.0f, 1.0f),		vertexColor };
		pVertices[4]	= { tcu::Vec4(posIncrement, posIncrement, 1.0f, 1.0f),		vertexColor };
		pVertices[5]	= { tcu::Vec4(posIncrement, -posIncrement, 1.0f, 1.0f),		vertexColor };

		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// Prepare buffers
	const vk::VkDeviceSize									minUboAlignment			= deviceLimits.minUniformBufferOffsetAlignment;
	const deUint32											bufferElementSizeVec4	= (deUint32)sizeof(tcu::Vec4);
	const deUint32											bufferElementSizeMat4	= (deUint32)sizeof(tcu::Mat4);
	deUint32												dynamicAlignmentVec4	= bufferElementSizeVec4;
	deUint32												dynamicAlignmentMat4	= bufferElementSizeMat4;

	if (minUboAlignment > 0)
	{
		dynamicAlignmentVec4	= (dynamicAlignmentVec4 + (deUint32)minUboAlignment - 1) & ~((deUint32)minUboAlignment - 1);
		dynamicAlignmentMat4	= (dynamicAlignmentMat4 + (deUint32)minUboAlignment - 1) & ~((deUint32)minUboAlignment - 1);
	}

	const deUint32											bufferSizeVec4			= m_numInstances * dynamicAlignmentVec4;
	const deUint32											bufferSizeMat4			= m_numInstances * dynamicAlignmentMat4;

	const Unique<VkBuffer>									uboBufferVertex			(makeBuffer(vk, device, bufferSizeVec4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
	const Unique<VkBuffer>									uboBufferShared			(makeBuffer(vk, device, bufferSizeVec4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
	const Unique<VkBuffer>									ssboBufferWrite			(makeBuffer(vk, device, bufferSizeVec4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	const Unique<VkBuffer>									uboBufferFrag			(makeBuffer(vk, device, bufferSizeMat4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
	const Unique<VkBuffer>									ssboBufferRead			(makeBuffer(vk, device, bufferSizeMat4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

	const UniquePtr<Allocation>								uboBufferAllocVertex	(bindBuffer(vk, device, allocator, *uboBufferVertex, MemoryRequirement::HostVisible));
	const UniquePtr<Allocation>								uboBufferAllocShared	(bindBuffer(vk, device, allocator, *uboBufferShared, MemoryRequirement::HostVisible));
	const UniquePtr<Allocation>								ssboBufferAllocWrite	(bindBuffer(vk, device, allocator, *ssboBufferWrite, MemoryRequirement::HostVisible));
	const UniquePtr<Allocation>								uboBufferAllocFrag		(bindBuffer(vk, device, allocator, *uboBufferFrag, MemoryRequirement::HostVisible));
	const UniquePtr<Allocation>								ssboBufferAllocRead		(bindBuffer(vk, device, allocator, *ssboBufferRead, MemoryRequirement::HostVisible));

	const float												colorIncrement			= 1.0f / float(m_numInstances);

	std::vector<tcu::Vec4>									constVertexOffsets;

	deUint32												columnCount				= 0u;
	float													columnOffset			= posIncrement;
	float													rowOffset				= -1.0f + posIncrement;

	for (deUint32 posId = 0; posId < m_numInstances; posId++)
	{
		constVertexOffsets.push_back(tcu::Vec4(-1.0f + columnOffset, rowOffset, 0.0f, 0.0f));

		columnOffset += 2 * posIncrement;
		columnCount++;

		if (columnCount >= instanceSize)
		{
			columnCount		= 0;
			columnOffset	= posIncrement;
			rowOffset		 += 2 * posIncrement;
		}
	}

	// Fill buffers
	{
		char*	pPosUboVertex	= static_cast<char*>(uboBufferAllocVertex->getHostPtr());
		char*	pPosUboShared	= static_cast<char*>(uboBufferAllocShared->getHostPtr());
		char*	pPosSsboWrite	= static_cast<char*>(ssboBufferAllocWrite->getHostPtr());
		char*	pPosUboFrag		= static_cast<char*>(uboBufferAllocFrag->getHostPtr());
		char*	pPosSsboRead	= static_cast<char*>(ssboBufferAllocRead->getHostPtr());

		if (m_testAllOffsets)
		{
			for (deUint32 posId = 0; posId < m_numInstances; posId++)
			{
				const float	constFragMat[]		=
				{
					colorIncrement, colorIncrement, colorIncrement, colorIncrement,
					colorIncrement, colorIncrement, colorIncrement, colorIncrement,
					colorIncrement, colorIncrement, colorIncrement, colorIncrement,
					colorIncrement, colorIncrement, colorIncrement, colorIncrement * float(posId + 1u)
				};

				const float	constReadMat[]		=
				{
					1.0f, 0.0f, 1.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 1.0f - colorIncrement * float(posId + 1u),
					1.0f, 0.0f, 1.0f, 0.17f,
					0.0f, 1.0f, 0.0f, 1.0f
				};

				*((tcu::Vec4*)pPosUboVertex)	= constVertexOffsets[posId];
				*((tcu::Vec4*)pPosUboShared)	= tcu::Vec4(colorIncrement * float(posId + 1u), 0.0f, 0.0f, 1.0f);
				*((tcu::Vec4*)pPosSsboWrite)	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
				*((tcu::Mat4*)pPosUboFrag)		= tcu::Mat4(constFragMat);
				*((tcu::Mat4*)pPosSsboRead)		= tcu::Mat4(constReadMat);
				pPosUboVertex					+= dynamicAlignmentVec4;
				pPosUboShared					+= dynamicAlignmentVec4;
				pPosSsboWrite					+= dynamicAlignmentVec4;
				pPosUboFrag						+= dynamicAlignmentMat4;
				pPosSsboRead					+= dynamicAlignmentMat4;
			}
		}
		else
		{
			for (deUint32 posId = 0; posId < m_numInstances; posId++)
			{
				const float	constFragMat[]		=
				{
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, m_fragUboOffset == posId ? 1.0f : 0.0f
				};

				const float	constReadMat[]		=
				{
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, m_ssboReadOffset == posId ? 0.25f : 0.0f,
					0.0f, 0.0f, 0.0f, m_ssboReadOffset == posId ? 0.17f : 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f
				};

				*((tcu::Vec4*)pPosUboVertex)	= constVertexOffsets[posId];
				*((tcu::Vec4*)pPosUboShared)	= m_sharedUboOffset == posId ? tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f) : tcu::Vec4(0);
				*((tcu::Vec4*)pPosSsboWrite)	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
				*((tcu::Mat4*)pPosUboFrag)		= tcu::Mat4(constFragMat);
				*((tcu::Mat4*)pPosSsboRead)		= tcu::Mat4(constReadMat);
				pPosUboVertex					+= dynamicAlignmentVec4;
				pPosUboShared					+= dynamicAlignmentVec4;
				pPosSsboWrite					+= dynamicAlignmentVec4;
				pPosUboFrag						+= dynamicAlignmentMat4;
				pPosSsboRead					+= dynamicAlignmentMat4;
			}
		}

		flushAlloc(vk, device, *uboBufferAllocVertex);
		flushAlloc(vk, device, *uboBufferAllocShared);
		flushAlloc(vk, device, *ssboBufferAllocWrite);
		flushAlloc(vk, device, *uboBufferAllocFrag);
		flushAlloc(vk, device, *ssboBufferAllocRead);
	}

	const vk::VkDescriptorBufferInfo						uboInfoVertexVec		= makeDescriptorBufferInfo(*uboBufferVertex, 0u, bufferElementSizeVec4);
	const vk::VkDescriptorBufferInfo						uboInfoVec				= makeDescriptorBufferInfo(*uboBufferShared, 0u, bufferElementSizeVec4);
	const vk::VkDescriptorBufferInfo						ssboInfoVec				= makeDescriptorBufferInfo(*ssboBufferWrite, 0u, bufferElementSizeVec4);
	const vk::VkDescriptorBufferInfo						uboInfoMat				= makeDescriptorBufferInfo(*uboBufferFrag, 0u, bufferElementSizeMat4);
	const vk::VkDescriptorBufferInfo						ssboInfoMat				= makeDescriptorBufferInfo(*ssboBufferRead, 0u, bufferElementSizeMat4);

	// Update descriptors
	DescriptorSetUpdateBuilder								builder;

	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(m_reverseOrder ? 4u : 0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &uboInfoVertexVec);
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(m_reverseOrder ? 3u : 1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &uboInfoVec);
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(						2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, &ssboInfoVec);
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(m_reverseOrder ? 1u : 3u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &uboInfoMat);
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(m_reverseOrder ? 0u : 4u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, &ssboInfoMat);
	builder.update(vk, device);

	// Command buffer
	const Unique<VkCommandPool>								cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>							cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkDeviceSize										vertexBufferOffset		= 0u;

	// Render result buffer
	const VkDeviceSize										colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(OUTPUT_COLOR_FORMAT)) * static_cast<VkDeviceSize>(m_renderSize.x()) * static_cast<VkDeviceSize>(m_renderSize.y());
	const Unique<VkBuffer>									colorBuffer				(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>								colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const VkClearValue										clearColorValue			= defaultClearValue(OUTPUT_COLOR_FORMAT);

	bool													runGraphics				= !m_runComputeFirst;

	for (int i = 0; i < 2; i++)
	{
		beginCommandBuffer(vk, *cmdBuffer);

		if (runGraphics)
		{
			renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), clearColorValue);
			graphicsPipeline.bind(*cmdBuffer);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		}
		else
		{
			computePipeline.bind(*cmdBuffer);
		}

		if (m_testAllOffsets)
		{
			for (deUint32 instance = 0; instance < m_numInstances; instance++)
			{
				deUint32				offsetVec4 = dynamicAlignmentVec4 * instance;
				deUint32				offsetMat4 = dynamicAlignmentMat4 * instance;
				std::vector<deUint32>	offsets;

				offsets.push_back(m_reverseOrder ? offsetMat4 : offsetVec4);
				offsets.push_back(m_reverseOrder ? offsetMat4 : offsetVec4);
				offsets.push_back(offsetVec4);
				offsets.push_back(m_reverseOrder ? offsetVec4 : offsetMat4);
				offsets.push_back(m_reverseOrder ? offsetVec4 : offsetMat4);

				if (runGraphics)
				{
					vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), (deUint32)offsets.size(), offsets.data());
					vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
				}
				else
				{
					vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), (deUint32)offsets.size(), offsets.data());
					vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
				}
			}
		}
		else
		{
			std::vector<deUint32>	offsets;

			offsets.push_back(m_reverseOrder ? dynamicAlignmentMat4 * m_ssboReadOffset : dynamicAlignmentVec4 * m_vertexOffset);
			offsets.push_back(m_reverseOrder ? dynamicAlignmentMat4 * m_fragUboOffset : dynamicAlignmentVec4 * m_sharedUboOffset);
			offsets.push_back(dynamicAlignmentVec4 * m_ssboWriteOffset);
			offsets.push_back(m_reverseOrder ? dynamicAlignmentVec4 * m_sharedUboOffset : dynamicAlignmentMat4 * m_fragUboOffset);
			offsets.push_back(m_reverseOrder ? dynamicAlignmentVec4 * m_vertexOffset : dynamicAlignmentMat4 * m_ssboReadOffset);

			if (runGraphics)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), (deUint32)offsets.size(), offsets.data());
				vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
			}
			else
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), (deUint32)offsets.size(), offsets.data());
				vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
			}
		}

		if (runGraphics)
		{
			renderPass.end(vk, *cmdBuffer);
			copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, m_renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		}

		runGraphics = !runGraphics;

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		m_context.resetCommandPoolForVKSC(device, *cmdPool);
	}

	// Check result image
	{
		tcu::TextureLevel				referenceTexture	(mapVkFormat(OUTPUT_COLOR_FORMAT), m_renderSize.x(), m_renderSize.y());
		const tcu::PixelBufferAccess	referenceAccess		= referenceTexture.getAccess();
		const deUint32					segmentSize			= m_renderSize.x() / instanceSize;

		// Create reference image
		if (m_testAllOffsets)
		{
			for (int y = 0; y < m_renderSize.y(); ++y)
			{
				for (int x = 0; x < m_renderSize.x(); ++x)
				{
					// While running test for all offsets, we create a nice gradient-like color for the pixels.
					float colorValue = (float)(y / segmentSize * instanceSize + x / segmentSize + 1u) * colorIncrement;

					referenceAccess.setPixel(tcu::Vec4(colorValue, 0.5f, colorValue, 1.0f), x, y);
				}
			}
		}
		else
		{
			// At first we have to find a correct location for the drawn square.
			const deUint32	segmentCountPerRow	= (deUint32)m_renderSize.x() / segmentSize;
			const deUint32	offsetY				= m_vertexOffset > segmentCountPerRow ? m_vertexOffset / segmentCountPerRow : 0u;
			const deUint32	offsetX				= offsetY > 0 ? m_vertexOffset - (segmentCountPerRow * offsetY) : m_vertexOffset;
			const deUint32	pixelOffsetY		= segmentSize * offsetY;
			const deUint32	pixelOffsetX		= segmentSize * offsetX;

			for (int y = 0; y < m_renderSize.y(); ++y)
			{
				for (int x = 0; x < m_renderSize.x(); ++x)
				{
					float colorValueRed		= clearColorValue.color.float32[0];
					float colorValueGreen	= clearColorValue.color.float32[1];
					float colorValueBlue	= clearColorValue.color.float32[2];

					// Next, we fill the correct number of pixels with test color.
					if (x >= (int)pixelOffsetX && x < int(pixelOffsetX + segmentSize) && y >= (int)pixelOffsetY && y < int(pixelOffsetY + segmentSize))
					{
						// While running test only for one offset, the result color for pixel is constant.
						colorValueRed	= 1.0f;
						colorValueGreen	= 0.5f;
						colorValueBlue	= colorValueRed;
					}

					referenceAccess.setPixel(tcu::Vec4(colorValueRed, colorValueGreen, colorValueBlue, 1.0f), x, y);
				}
			}
		}

		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess resultPixelAccess(mapVkFormat(OUTPUT_COLOR_FORMAT), m_renderSize.x(), m_renderSize.y(), 1, colorBufferAlloc->getHostPtr());

		if (!tcu::floatThresholdCompare(log, "color", "Image compare", referenceAccess, resultPixelAccess, tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered image is not correct");
	}

	// Check result buffer values
	{
		invalidateAlloc(vk, device, *ssboBufferAllocWrite);

		std::vector<tcu::Vec4>	refColors;
		std::vector<tcu::Vec4>	outColors;

			for (deUint32 i = 0; i < m_numInstances; i++)
			{
				if (m_testAllOffsets)
				{
					refColors.push_back(tcu::Vec4(float(i + 1) * colorIncrement, 1.0f - float(i + 1) * colorIncrement, 0.17f, 1.0f));
				}
				else
				{
					refColors.push_back(m_ssboWriteOffset == i ? tcu::Vec4(1.0f, 0.25f, 0.17f, 1.0f) : tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
				}

				outColors.push_back(*(tcu::Vec4*)((deUint8*)ssboBufferAllocWrite->getHostPtr() + dynamicAlignmentVec4 * i));

				if (!compareVectors(outColors[i], refColors[i], 0.01f))
				{
					log << tcu::TestLog::Message << "Reference: " << refColors[i].x() << ", " << refColors[i].y() << ", " << refColors[i].z() << ", " << refColors[i].w() << ", " << tcu::TestLog::EndMessage;
					log << tcu::TestLog::Message << "Result   : " << outColors[i].x() << ", " << outColors[i].y() << ", " << outColors[i].z() << ", " << outColors[i].w() << ", " << tcu::TestLog::EndMessage;

					return tcu::TestStatus::fail("Result value is not correct");
				}
			}
		}

	return tcu::TestStatus::pass("Success");
}

class DynamicOffsetMixedTest : public vkt::TestCase
{
public:
							DynamicOffsetMixedTest	(tcu::TestContext&	testContext,
													 const PipelineConstructionType	pipelineConstructionType,
													 const std::string&				name,
													 const std::string&				description,
													 const tcu::IVec2				renderSize,
													 const deUint32					numInstances,
													 const bool						testAllOffsets,
													 const bool						reverseOrder,
													 const bool						runComputeFirst	= false,
													 const deUint32					vertexOffset	= 0u,
													 const deUint32					sharedUboOffset	= 0u,
													 const deUint32					fragUboOffset	= 0u,
													 const deUint32					ssboReadOffset	= 0u,
													 const deUint32					ssboWriteOffset	= 0u)
													: vkt::TestCase					(testContext, name, description)
													, m_pipelineConstructionType	(pipelineConstructionType)
													, m_renderSize					(renderSize)
													, m_numInstances				(numInstances)
													, m_testAllOffsets				(testAllOffsets)
													, m_reverseOrder				(reverseOrder)
													, m_runComputeFirst				(runComputeFirst)
													, m_vertexOffset				(vertexOffset)
													, m_sharedUboOffset				(sharedUboOffset)
													, m_fragUboOffset				(fragUboOffset)
													, m_ssboReadOffset				(ssboReadOffset)
													, m_ssboWriteOffset				(ssboWriteOffset)
													{}

						~DynamicOffsetMixedTest		(void);

	void				initPrograms				(SourceCollections& sourceCollections) const;
	void				checkSupport				(vkt::Context& context) const;
	TestInstance		*createInstance				(Context& context) const;
private:
	const PipelineConstructionType	m_pipelineConstructionType;
	const tcu::IVec2				m_renderSize;
	const deUint32					m_numInstances;
	const bool						m_testAllOffsets;
	const bool						m_reverseOrder;
	const bool						m_runComputeFirst;
	const deUint32					m_vertexOffset;
	const deUint32					m_sharedUboOffset;
	const deUint32					m_fragUboOffset;
	const deUint32					m_ssboReadOffset;
	const deUint32					m_ssboWriteOffset;
};

DynamicOffsetMixedTest::~DynamicOffsetMixedTest (void)
{
}

void DynamicOffsetMixedTest::initPrograms (SourceCollections& sourceCollections) const
{
	// Vertex
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(set = 0, binding = " << (m_reverseOrder ? "4" : "0") << ") uniform uboVertexData\n"
			<< "{\n"
			<< "	vec4 position;\n"
			<< "} inputPosData;\n"
			<< "\n"
			<< "layout(location = 0) in vec4 inPosition;\n"
			<< "layout(location = 1) in vec4 inColor;\n"
			<< "layout(location = 0) out vec4 outColor;\n"
			<< "\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	gl_Position = inPosition + inputPosData.position;\n"
			<< "	outColor = inColor;\n"
			<< "}\n";

		sourceCollections.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(set = 0, binding = " << (m_reverseOrder ? "3" : "1") << ") uniform uboSharedData\n"
			<< "{\n"
			<< "	vec4 color;\n"
			<< "} inputData0;\n"
			<< "\n"
			<< "layout(set = 0, binding = " << (m_reverseOrder ? "1" : "3") << ") uniform uboFragOnly\n"
			<< "{\n"
			<< "	mat4 color;\n"
			<< "} inputData1;\n"
			<< "\n"
			<< "layout(location = 0) in vec4 inColor;\n"
			<< "layout(location = 0) out vec4 outColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	outColor = inColor + inputData0.color;\n"
			<< "	outColor.b = inputData1.color[3][3];\n"
			<< "}\n";

		sourceCollections.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	// Compute
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(set = 0, binding = " << (m_reverseOrder ? "3" : "1") << ") uniform uboSharedData\n"
			<< "{\n"
			<< "	vec4 color;\n"
			<< "} inputData;\n"
			<< "\n"
			<< "layout(set = 0, binding = 2) writeonly buffer ssboOutput\n"
			<< "{\n"
			<< "	vec4 color;\n"
			<< "} outData;\n"
			<< "\n"
			<< "layout(set = 0, binding = " << (m_reverseOrder ? "0" : "4") << ") readonly buffer ssboInput\n"
			<< "{\n"
			<< "	mat4 color;\n"
			<< "} readData;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	outData.color = inputData.color;\n"
			<< "	outData.color.g = readData.color[3][1];\n"
			<< "	outData.color.b = readData.color[3][2];\n"
			<< "}\n";

		sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
	}
}

void DynamicOffsetMixedTest::checkSupport (vkt::Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

TestInstance* DynamicOffsetMixedTest::createInstance (Context& context) const
{
	return new DynamicOffsetMixedTestInstance	(context,
												 m_pipelineConstructionType,
												 m_renderSize,
												 m_numInstances,
												 m_testAllOffsets,
												 m_reverseOrder,
												 m_runComputeFirst,
												 m_vertexOffset,
												 m_sharedUboOffset,
												 m_fragUboOffset,
												 m_ssboReadOffset,
												 m_ssboWriteOffset);
}

} // anonymous

tcu::TestCaseGroup* createDynamicOffsetTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	const char*	pipelineTypes[]			= { "graphics", "compute" };

	struct
	{
		const char*				name;
		const GroupingStrategy	strategy;
	}
	const groupingTypes[]				=
	{
		{ "single_set",		GroupingStrategy::SINGLE_SET	},
		{ "multiset",		GroupingStrategy::MULTISET		},
		{ "arrays",			GroupingStrategy::ARRAYS		},
	};

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

	for (deUint32 pipelineTypeIdx = 0; pipelineTypeIdx < DE_LENGTH_OF_ARRAY(pipelineTypes); pipelineTypeIdx++)
	{
		// VK_EXT_graphics_pipeline_library can't be tested with compute pipeline
		if ((pipelineTypeIdx == 1) && (pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
			continue;

		de::MovePtr<tcu::TestCaseGroup>	pipelineTypeGroup	(new tcu::TestCaseGroup(testCtx, pipelineTypes[pipelineTypeIdx], ""));

		for (deUint32 groupingTypeIdx = 0; groupingTypeIdx < DE_LENGTH_OF_ARRAY(groupingTypes); ++groupingTypeIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> groupingTypeGroup (new tcu::TestCaseGroup(testCtx, groupingTypes[groupingTypeIdx].name, ""));

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
									TestParams params
									{
										pipelineConstructionType,
										descriptorTypes[descriptorTypeIdx].type,
										numCmdBuffers[numCmdBuffersIdx].num,
										reverseOrders[reverseOrderIdx].reverse,
										numDescriptorSetBindings[numDescriptorSetBindingsIdx].num,
										numDynamicBindings[numDynamicBindingsIdx].num,
										numNonDynamicBindings[numNonDynamicBindingsIdx].num,
										groupingTypes[groupingTypeIdx].strategy
									};
#ifndef CTS_USES_VULKANSC
									if (strcmp(pipelineTypes[pipelineTypeIdx], "graphics") == 0)
										numDynamicBindingsGroup->addChild(new DynamicOffsetGraphicsTest(testCtx, numNonDynamicBindings[numNonDynamicBindingsIdx].name, "", params));
									else
#endif // CTS_USES_VULKANSC
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

				groupingTypeGroup->addChild(descriptorTypeGroup.release());
			}

			pipelineTypeGroup->addChild(groupingTypeGroup.release());
		}

		dynamicOffsetTests->addChild(pipelineTypeGroup.release());
	}

	// Dynamic descriptor offset test for combined descriptor sets.
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) {
		de::MovePtr<tcu::TestCaseGroup>	combinedDescriptorsTests(new tcu::TestCaseGroup(testCtx, "combined_descriptors", ""));

		struct
		{
			const char* name;
			const bool	reverseDescriptors;
		}
		const orders[] =
		{
			{	"same_order",		false	},
			{	"reverse_order",	true	}
		};

		struct
		{
			const char* name;
			const deUint32	offsetCount;
			const deUint32	offsets[5];
		}
		const numOffsets[] =
		{
			{	"16",		16u,	{ 15u, 7u, 2u, 3u, 5u			}	},
			{	"64",		64u,	{ 27u, 22u, 45u, 19u, 59u		}	},
			{	"256",		256u,	{ 197u, 244u, 110u, 238u, 88u	}	}
		};

		struct
		{
			const char* name;
			const bool	computeFirst;
		}
		const pipelineOrders[] =
		{
			{	"graphics_first",	false	},
			{	"compute_first",	true	}
		};

		// Run tests for all offsets
		{
			de::MovePtr<tcu::TestCaseGroup>	allOffsetsGroup(new tcu::TestCaseGroup(testCtx, "all_offsets", ""));
			de::MovePtr<tcu::TestCaseGroup>	singleOffsetGroup(new tcu::TestCaseGroup(testCtx, "single_offset", ""));

			for (const auto& order : orders)
			{
				for (const auto& offsets : numOffsets)
				{
					for (const auto& pipeline : pipelineOrders)
					{
						allOffsetsGroup->addChild(new DynamicOffsetMixedTest(
							testCtx,
							pipelineConstructionType,
							std::string(order.name) + "_" + std::string(offsets.name) + "_" + pipeline.name,
							"",
							tcu::IVec2(32, 32),		// Render size
							offsets.offsetCount,
							true,					// All offsets
							order.reverseDescriptors,
							pipeline.computeFirst));
						singleOffsetGroup->addChild(new DynamicOffsetMixedTest(
							testCtx,
							pipelineConstructionType,
							std::string(order.name) + "_" + std::string(offsets.name) + "_" + pipeline.name,
							"",
							tcu::IVec2(32, 32),		// Render size
							offsets.offsetCount,
							false,					// Single offset only
							order.reverseDescriptors,
							pipeline.computeFirst,
							offsets.offsets[0],		// For vertex ubo
							offsets.offsets[1],		// For shared ubo (fragment & compute)
							offsets.offsets[2],		// For fragment ubo
							offsets.offsets[3],		// For ssbo read only
							offsets.offsets[4]));	// For ssbo write only
					}
				}
			}
			combinedDescriptorsTests->addChild(allOffsetsGroup.release());
			combinedDescriptorsTests->addChild(singleOffsetGroup.release());
		}

		dynamicOffsetTests->addChild(combinedDescriptorsTests.release());
	}

	return dynamicOffsetTests.release();
}

} // pipeline
} // vkt
