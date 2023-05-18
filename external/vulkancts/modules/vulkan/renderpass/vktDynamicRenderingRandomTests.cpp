/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google LLC
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Vulkan Dynamic Rendering Random Tests
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDynamicRenderingRandomTests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"

#include <iostream>

namespace vkt
{
namespace renderpass
{
namespace
{

using namespace vk;

using tcu::UVec2;
using tcu::Vec4;

const deUint32	maxQueries		= 50u * 3u; // multiview with 3 bits can be used
const deUint32	numLayers		= 4u;

struct TestParameters
{
	VkFormat	imageFormat;
	UVec2		renderSize;
	bool		enableGeometry;
	bool		enableTessellation;
	deUint32	randomSeed;
};

struct PushConstantData
{
	Vec4	scale;
	Vec4	offset;
	Vec4	color;
	deInt32	layer;
};

class DynamicRenderingTestInstance : public TestInstance
{
public:
								DynamicRenderingTestInstance	(Context&				context,
																 const TestParameters&	parameters);
protected:
	virtual tcu::TestStatus		iterate							(void);

	TestParameters				m_parameters;
	Move<VkImage>				m_imageColor;
	Move<VkImageView>			m_colorAttachmentView;
	de::MovePtr<Allocation>		m_imageColorAlloc;
	de::SharedPtr<Draw::Buffer>	m_imageBuffer;
	deUint32					m_layerSizeBytes;
	Move<VkShaderModule>		m_vertexModule;
	Move<VkShaderModule>		m_vertexPassthroughModule;
	Move<VkShaderModule>		m_vertexLayerModule;
	Move<VkShaderModule>		m_geometryModule;
	Move<VkShaderModule>		m_geometryLayerModule;
	Move<VkShaderModule>		m_tscModule;
	Move<VkShaderModule>		m_tseModule;
	Move<VkShaderModule>		m_fragmentModule;
	de::SharedPtr<Draw::Buffer>	m_vertexBuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	de::SharedPtr<Draw::Buffer>	m_queryResults;
	Move<VkQueryPool>			m_queryPool;
	Move<VkCommandPool>			m_cmdPool;
	de::Random					m_random;
	Move<VkPipeline>			m_pipelineBasic;
	Move<VkPipeline>			m_pipelineGeom;
	Move<VkPipeline>			m_pipelineTess;
	Move<VkPipeline>			m_pipelineLayer;
	Move<VkPipeline>			m_pipelineMultiview;
};

DynamicRenderingTestInstance::DynamicRenderingTestInstance (Context&				context,
															const TestParameters&	parameters)
: TestInstance	(context)
, m_parameters	(parameters)
, m_random		(parameters.randomSeed)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				device		= m_context.getDevice();
	Allocator&					allocator	= m_context.getDefaultAllocator();

	m_cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex());

	// Vertices.
	{
		std::vector<Vec4>			vertices	=
		{
			Vec4(-0.5f,  0.5f, 0.0f, 1.0f),
			Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
			Vec4( 0.5f,  0.5f, 0.0f, 1.0f),
			Vec4( 0.5f, -0.5f, 0.0f, 1.0f)
		};

		const VkDeviceSize			bufferSize	= vertices.size() * sizeof(vertices[0]);
		const VkBufferCreateInfo	bufferInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexBuffer	= Draw::Buffer::createAndAlloc(vk, device, bufferInfo, allocator, MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), vertices.data(), static_cast<std::size_t>(bufferSize));
		flushAlloc(vk, device, m_vertexBuffer->getBoundMemory());
	}

	// Create color image.
	{
		const VkImageUsageFlags			imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
															  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
															  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		m_layerSizeBytes									= m_parameters.renderSize.x() *
															  m_parameters.renderSize.y() *
															  tcu::getPixelSize(mapVkFormat(m_parameters.imageFormat));
		const VkDeviceSize				imageBufferSize		= m_layerSizeBytes * numLayers;

		const VkImageCreateInfo			imageInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			(VkImageCreateFlags)0,							// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType
			m_parameters.imageFormat,						// VkFormat					format
			makeExtent3D(m_parameters.renderSize.x(),
						 m_parameters.renderSize.y(), 1),	// VkExtent3D				extent
			1u,												// deUint32					mipLevels
			numLayers,										// deUint32					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling
			imageUsage,										// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode
			0u,												// deUint32					queueFamilyIndexCount
			DE_NULL,										// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout
		};

		const VkBufferCreateInfo		bufferInfo			= makeBufferCreateInfo(imageBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		m_imageColor		= makeImage(vk, device, imageInfo);
		m_imageColorAlloc	= bindImage(vk, device, allocator, *m_imageColor, MemoryRequirement::Any);
		m_imageBuffer		= Draw::Buffer::createAndAlloc(vk, device, bufferInfo, allocator, MemoryRequirement::HostVisible);

		const VkImageSubresourceRange	imageSubresource	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numLayers);

		m_colorAttachmentView	= makeImageView(vk, device, *m_imageColor, VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_parameters.imageFormat, imageSubresource);

		const Allocation				alloc				= m_imageBuffer->getBoundMemory();
		deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(imageBufferSize));
		flushAlloc(vk, device, alloc);
	}

	// Pipeline layout.
	{
		const auto	pcStages	= VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		const auto	pcSize		= static_cast<uint32_t>(sizeof(PushConstantData));
		const auto	pcRange		= makePushConstantRange(pcStages, 0u, pcSize);

		m_pipelineLayout = makePipelineLayout(vk, device, 0u, DE_NULL, 1u, &pcRange);
	}

	// Shader modules.
	{
		m_vertexModule				= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
		if (m_parameters.enableGeometry)
		{
			m_vertexPassthroughModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("vertPassthrough"), 0u);
			m_vertexLayerModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vertLayer"), 0u);
			m_geometryModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
			m_geometryLayerModule		= createShaderModule(vk, device, m_context.getBinaryCollection().get("geomLayer"), 0u);
		}
		if (m_parameters.enableTessellation)
		{
			m_tscModule					= createShaderModule(vk, device, m_context.getBinaryCollection().get("tsc"), 0u);
			m_tseModule					= createShaderModule(vk, device, m_context.getBinaryCollection().get("tse"), 0u);
		}
	}

	// Pipelines.
	{
		const std::vector<vk::VkViewport>	viewports			(1u, vk::makeViewport(m_parameters.renderSize));
		const std::vector<vk::VkRect2D>		scissors			(1u, vk::makeRect2D(m_parameters.renderSize));

		VkPipelineRenderingCreateInfoKHR	renderingCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,	// VkStructureType	sType
			DE_NULL,												// const void*		pNext
			0u,														// deUint32			viewMask
			1u,														// deUint32			colorAttachmentCount
			&m_parameters.imageFormat,								// const VkFormat*	pColorAttachmentFormats
			VK_FORMAT_UNDEFINED,									// VkFormat			depthAttachmentFormat
			VK_FORMAT_UNDEFINED,									// VkFormat			stencilAttachmentFormat
		};

		m_pipelineBasic		= makeGraphicsPipeline(vk,										// const DeviceInterface&							vk
												   device,									// const VkDevice									device
												   *m_pipelineLayout,						// const VkPipelineLayout							pipelineLayout
												   *m_vertexModule,							// const VkShaderModule								vertexShaderModule
												   DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
												   DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
												   DE_NULL,									// const VkShaderModule								geometryShaderModule
												   *m_fragmentModule,						// const VkShaderModule								fragmentShaderModule
												   DE_NULL,									// const VkRenderPass								renderPass
												   viewports,								// const std::vector<VkViewport>&					viewports
												   scissors,								// const std::vector<VkRect2D>&						scissors
												   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
												   0u,										// const deUint32									subpass
												   0u,										// const deUint32									patchControlPoints
												   DE_NULL,									// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												   DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												   DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												   DE_NULL,									// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												   DE_NULL,									// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
												   DE_NULL,									// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
												   &renderingCreateInfo);					// const void*										pNext

		if (m_parameters.enableGeometry)
		{
			m_pipelineGeom = makeGraphicsPipeline(
					vk,									// const DeviceInterface&							vk
					device,								// const VkDevice									device
					*m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
					*m_vertexPassthroughModule,			// const VkShaderModule								vertexShaderModule
					DE_NULL,							// const VkShaderModule								tessellationControlShaderModule
					DE_NULL,							// const VkShaderModule								tessellationEvalShaderModule
					*m_geometryModule,					// const VkShaderModule								geometryShaderModule
					*m_fragmentModule,					// const VkShaderModule								fragmentShaderModule
					DE_NULL,							// const VkRenderPass								renderPass
					viewports,							// const std::vector<VkViewport>&					viewports
					scissors,							// const std::vector<VkRect2D>&						scissors
					VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology						topology
					0u,									// const deUint32									subpass
					0u,									// const deUint32									patchControlPoints
					DE_NULL,							// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
					DE_NULL,							// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
					DE_NULL,							// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
					DE_NULL,							// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
					DE_NULL,							// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
					DE_NULL,							// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
					&renderingCreateInfo);				// const void*										pNext

			m_pipelineLayer = makeGraphicsPipeline(
					vk,										// const DeviceInterface&							vk
					device,									// const VkDevice									device
					*m_pipelineLayout,						// const VkPipelineLayout							pipelineLayout
					*m_vertexLayerModule,					// const VkShaderModule								vertexShaderModule
					DE_NULL,								// const VkShaderModule								tessellationControlShaderModule
					DE_NULL,								// const VkShaderModule								tessellationEvalShaderModule
					*m_geometryLayerModule,					// const VkShaderModule								geometryShaderModule
					*m_fragmentModule,						// const VkShaderModule								fragmentShaderModule
					DE_NULL,								// const VkRenderPass								renderPass
					viewports,								// const std::vector<VkViewport>&					viewports
					scissors,								// const std::vector<VkRect2D>&						scissors
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
					0u,										// const deUint32									subpass
					0u,										// const deUint32									patchControlPoints
					DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
					DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
					DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
					DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
					DE_NULL,								// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
					DE_NULL,								// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
					&renderingCreateInfo);					// const void*										pNext
		}

		if (m_parameters.enableTessellation)
		{
			m_pipelineTess = makeGraphicsPipeline(
					vk,										// const DeviceInterface&							vk
					device,									// const VkDevice									device
					*m_pipelineLayout,						// const VkPipelineLayout							pipelineLayout
					*m_vertexModule,						// const VkShaderModule								vertexShaderModule
					*m_tscModule,							// const VkShaderModule								tessellationControlShaderModule
					*m_tseModule,							// const VkShaderModule								tessellationEvalShaderModule
					DE_NULL,								// const VkShaderModule								geometryShaderModule
					*m_fragmentModule,						// const VkShaderModule								fragmentShaderModule
					DE_NULL,								// const VkRenderPass								renderPass
					viewports,								// const std::vector<VkViewport>&					viewports
					scissors,								// const std::vector<VkRect2D>&						scissors
					vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,	// const VkPrimitiveTopology						topology
					0u,										// const deUint32									subpass
					4u,										// const deUint32									patchControlPoints
					DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
					DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
					DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
					DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
					DE_NULL,								// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
					DE_NULL,								// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
					&renderingCreateInfo);					// const void*										pNext
		}

		renderingCreateInfo.viewMask = 0xb; // 1011b
		m_pipelineMultiview	= makeGraphicsPipeline(vk,										// const DeviceInterface&							vk
												   device,									// const VkDevice									device
												   *m_pipelineLayout,						// const VkPipelineLayout							pipelineLayout
												   *m_vertexModule,							// const VkShaderModule								vertexShaderModule
												   DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
												   DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
												   DE_NULL,									// const VkShaderModule								geometryShaderModule
												   *m_fragmentModule,						// const VkShaderModule								fragmentShaderModule
												   DE_NULL,									// const VkRenderPass								renderPass
												   viewports,								// const std::vector<VkViewport>&					viewports
												   scissors,								// const std::vector<VkRect2D>&						scissors
												   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology						topology
												   0u,										// const deUint32									subpass
												   0u,										// const deUint32									patchControlPoints
												   DE_NULL,									// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												   DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												   DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												   DE_NULL,									// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												   DE_NULL,									// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
												   DE_NULL,									// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
												   &renderingCreateInfo);					// const void*										pNext
	}

	// Query result buffer.
	{
		const VkDeviceSize			bufferSize = 1024u;
		const VkBufferCreateInfo	bufferInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		m_queryResults = Draw::Buffer::createAndAlloc(vk, device, bufferInfo, allocator, MemoryRequirement::HostVisible);
	}

	// Query pool.
	{
		VkQueryPoolCreateInfo	queryPoolInfo	=
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType
			DE_NULL,									// const void*						pNext
			0,											// VkQueryPoolCreateFlags			flags
			vk::VK_QUERY_TYPE_OCCLUSION,				// VkQueryType						queryType
			maxQueries,									// uint32_t							queryCount
			0											// VkQueryPipelineStatisticFlags	pipelineStatistics
		};

		m_queryPool	= createQueryPool(vk, device, &queryPoolInfo);
	}
}


tcu::TestStatus DynamicRenderingTestInstance::iterate (void)
{
	const DeviceInterface&				vk				= m_context.getDeviceInterface();
	const VkDevice						device			= m_context.getDevice();
	const VkQueue						queue			= m_context.getUniversalQueue();
	const deUint32						queueFamily		= m_context.getUniversalQueueFamilyIndex();
	tcu::TestLog&						log				= m_context.getTestContext().getLog();

	std::vector<Move<VkCommandBuffer>>	cmdBuffers;
	std::vector<Move<VkCommandBuffer>>	secondaryCmdBuffers;
	std::vector<tcu::TextureLevel>		ref;

	for (deUint32 i = 0; i < numLayers; i++)
	{
		ref.emplace_back(mapVkFormat(m_parameters.imageFormat), m_parameters.renderSize.x(), m_parameters.renderSize.y(), 1u);

		tcu::PixelBufferAccess	access = ref[i].getAccess();

		for (deUint32 y = 0; y < m_parameters.renderSize.x(); y++)
			for (deUint32 x = 0; x < m_parameters.renderSize.x(); x++)
				access.setPixel(Vec4(0.0f), x, y);
	}

	cmdBuffers.push_back(allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	beginCommandBuffer(vk, *cmdBuffers.back());
	vk.cmdResetQueryPool(*cmdBuffers.back(), *m_queryPool, 0, maxQueries);

	clearColorImage(vk, device, queue, queueFamily, *m_imageColor, Vec4(0.0),
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, numLayers);

	VkRenderingFlagsKHR					prevFlags		= 0;
	Vec4								clearColor		= Vec4(0.0f);
	VkRect2D							renderArea		= makeRect2D(0, 0, 0, 0);
	VkAttachmentLoadOp					loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	deUint32							renderWidth		= 0u;
	deUint32							renderHeight	= 0u;
	deUint32							renderX			= 0u;
	deUint32							renderY			= 0u;
	deUint32							quadShrink		= 0u;
	deUint32							queryIndex		= 0u;
	std::vector<uint32_t>				queryMultiviewCount;

	enum PipelineType
	{
		PIPELINE_TYPE_VERTEX_FRAGMENT = 0,
		PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT = 1,
		PIPELINE_TYPE_VERTEX_TESS_FRAGMENT = 2,
		PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT_LAYER = 3,
		PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW = 4,
		PIPELINE_TYPE_ATTACHMENT_CLEAR = 5,
		PIPELINE_TYPE_MAX = 6
	} pipelineType = PIPELINE_TYPE_VERTEX_FRAGMENT;

	std::vector<VkPipeline>				pipelines		= {*m_pipelineBasic, *m_pipelineGeom, *m_pipelineTess, *m_pipelineLayer, *m_pipelineMultiview, *m_pipelineBasic};

	deUint32							validPipelines	= (1 << PIPELINE_TYPE_VERTEX_FRAGMENT) | (1 << PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW) | (1 << PIPELINE_TYPE_ATTACHMENT_CLEAR);

	if (m_parameters.enableGeometry)
		validPipelines |= (1 << PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT) | (1 << PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT_LAYER);

	if (m_parameters.enableTessellation)
		validPipelines |= (1 << PIPELINE_TYPE_VERTEX_TESS_FRAGMENT);

	const int							numIterations	= 50;

	for (int i = 0; i < numIterations; i++)
	{
		VkRenderingFlagsKHR		flags								= 0;
		bool					useSecondaryCmdBuffer				= m_random.getUint32() % 5 == 0;
		bool					bindPipelineBeforeBeginRendering	= m_random.getBool();

		if (useSecondaryCmdBuffer)
		{
			flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR;
			// Pipeline bind needs to go to the same command buffer that has the draw call.
			bindPipelineBeforeBeginRendering = false;
		}

		if (prevFlags & VK_RENDERING_SUSPENDING_BIT_KHR)
		{
			// Resuming: Don't touch the beginRendering parameters as they need to
			// match with the previous command buffer.

			flags |= VK_RENDERING_RESUMING_BIT_KHR;

			// Use a new command buffer.
			VK_CHECK(vk.endCommandBuffer(*cmdBuffers.back()));

			cmdBuffers.push_back(allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
			beginCommandBuffer(vk, *cmdBuffers.back());

			// Make the drawn quad smaller so the previous quad is still visible.
			quadShrink++;

			// Pipeline bind is not allowed between suspend and resume.
			bindPipelineBeforeBeginRendering = false;
		}
		else
		{
			// Not resuming: we can randomize new beginRendering parameters.
			clearColor		= Vec4(m_random.getFloat(), m_random.getFloat(), m_random.getFloat(), 1.0f);

			const deUint32 minAreaSize = 32u;
			// Use a render area with an even size to make the margin around the quad symmetrical.
			renderWidth		= (m_random.getUint32() % (m_parameters.renderSize.x() / 2 - minAreaSize) + minAreaSize) & (~1u);
			renderHeight	= (m_random.getUint32() % (m_parameters.renderSize.y() / 2 - minAreaSize) + minAreaSize) & (~1u);
			renderX			= m_random.getUint32() % (m_parameters.renderSize.x() - renderWidth);
			renderY			= m_random.getUint32() % (m_parameters.renderSize.y() - renderHeight);
			renderArea		= { makeOffset2D(renderX, renderY), makeExtent2D(renderWidth, renderHeight) };
			loadOp			= m_random.getBool() ? vk::VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			quadShrink		= 0u;
		}

		// Randomize pipeline type on every round. Multiview pipeline is an exception: the view mask cannot change
		// between suspend and resume.
		if (!(prevFlags & VK_RENDERING_SUSPENDING_BIT_KHR) || pipelineType != PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW)
		{
			deUint32 pipelineMask = validPipelines;

			if (prevFlags & VK_RENDERING_SUSPENDING_BIT_KHR)
			{
				// If resuming from non-multiview pipeline the new pipeline must also be non-multiview to keep the view mask.
				if (pipelineType != PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW)
					pipelineMask &= ~(1 << PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW);

				// The number of layers need to match too.
				const deUint32	layeredPipelines = (1 << PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT_LAYER) | (1 << PIPELINE_TYPE_ATTACHMENT_CLEAR);
				if ((1 << pipelineType) & layeredPipelines)
				{
					// Filter out all non-layered pipelines.
					pipelineMask &= layeredPipelines;
				}
				else
				{
					// Filter out all layered pipelines.
					pipelineMask &= ~layeredPipelines;
				}
			}

			do
			{
				pipelineType = static_cast<PipelineType>(m_random.getUint32() % PIPELINE_TYPE_MAX);
			} while (((1 << pipelineType) & pipelineMask) == 0);
		}

		const bool				occlusionQuery						= m_random.getBool() && pipelineType != PIPELINE_TYPE_ATTACHMENT_CLEAR;
		const deUint32			viewMask							= pipelineType == PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW ? 0xb /* 1011b */ : 0;
		const bool				useLayers							= pipelineType == PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT_LAYER || pipelineType == PIPELINE_TYPE_ATTACHMENT_CLEAR;
		const bool				suspend								= m_random.getUint32() % 5 == 0 && i != numIterations - 1;

		if (suspend)
		{
			flags |= VK_RENDERING_SUSPENDING_BIT_KHR;
		}

		const VkClearValue	clearValue								= makeClearValueColor(clearColor);

		if (bindPipelineBeforeBeginRendering)
			vk.cmdBindPipeline(*cmdBuffers.back(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineType]);

		// Begin rendering
		{
			const VkRenderingAttachmentInfoKHR	renderingAttachmentInfo	=
			{
				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType			sType
				DE_NULL,											// const void*				pNext
				*m_colorAttachmentView,								// VkImageView				imageView
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout			imageLayout
				VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode
				DE_NULL,											// VkImageView				resolveImageView
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout
				loadOp,												// VkAttachmentLoadOp		loadOp
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		storeOp
				clearValue											// VkClearValue				clearValue
			};

			const VkRenderingInfoKHR			renderingInfo			=
			{
				VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,				// VkStructureType						sType
				DE_NULL,											// const void*							pNext
				flags,												// VkRenderingFlagsKHR					flags
				renderArea,											// VkRect2D								renderArea
				useLayers ? numLayers : 1u,							// deUint32								layerCount
				viewMask,											// deUint32								viewMask
				1u,													// deUint32								colorAttachmentCount
				&renderingAttachmentInfo,							// const VkRenderingAttachmentInfoKHR*	pColorAttachments
				DE_NULL,											// const VkRenderingAttachmentInfoKHR*	pDepthAttachment
				DE_NULL												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment
			};

			vk.cmdBeginRendering(*cmdBuffers.back(), &renderingInfo);
		}

		if (useSecondaryCmdBuffer)
		{
			secondaryCmdBuffers.push_back(allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));

			const VkCommandBufferInheritanceRenderingInfoKHR	inheritanceRenderingInfo	=
			{
					VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType			sType
					DE_NULL,															// const void*				pNext
					flags & ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR,	// VkRenderingFlagsKHR		flags
					viewMask,															// uint32_t					viewMask
					1u,																	// uint32_t					colorAttachmentCount
					&m_parameters.imageFormat,											// const VkFormat*			pColorAttachmentFormats
					VK_FORMAT_UNDEFINED,												// VkFormat					depthAttachmentFormat
					VK_FORMAT_UNDEFINED,												// VkFormat					stencilAttachmentFormat
					VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	rasterizationSamples
			};

			const VkCommandBufferInheritanceInfo				bufferInheritanceInfo		=
			{
					vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,				// VkStructureType					sType
					&inheritanceRenderingInfo,											// const void*						pNext
					DE_NULL,															// VkRenderPass						renderPass
					0u,																	// deUint32							subpass
					DE_NULL,															// VkFramebuffer					framebuffer
					VK_FALSE,															// VkBool32							occlusionQueryEnable
					(VkQueryControlFlags)0u,											// VkQueryControlFlags				queryFlags
					(VkQueryPipelineStatisticFlags)0u									// VkQueryPipelineStatisticFlags	pipelineStatistics
			};

			const VkCommandBufferBeginInfo						commandBufBeginParams		=
			{
					VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,						// VkStructureType				sType
					DE_NULL,															// const void*					pNext
					VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
					VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,					// VkCommandBufferUsageFlags	flags
					&bufferInheritanceInfo
			};

			VK_CHECK(vk.beginCommandBuffer(*secondaryCmdBuffers.back(), &commandBufBeginParams));
		}

		const VkCommandBuffer&	cmdBuffer							= useSecondaryCmdBuffer ? *secondaryCmdBuffers.back() : *cmdBuffers.back();

		if (!bindPipelineBeforeBeginRendering)
			vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipelineType]);

		// Calculate push constant data.
		const float				scaleX								= static_cast<float>(renderArea.extent.width - quadShrink * 4) / static_cast<float>(m_parameters.renderSize.x());
		const float				scaleY								= static_cast<float>(renderArea.extent.height - quadShrink * 4) / static_cast<float>(m_parameters.renderSize.y());

		DE_ASSERT(scaleX > 0.0f);
		DE_ASSERT(scaleY > 0.0f);

		const float				pixelSizeX							= 2.0f / static_cast<float>(m_parameters.renderSize.x());
		const float				pixelSizeY							= 2.0f / static_cast<float>(m_parameters.renderSize.y());
		const Vec4				scale								= {scaleX + pixelSizeX * 0.5f, scaleY + pixelSizeY * 0.5f, 1.0f, 1.0f};
		const float				offsetX								= static_cast<float>(renderArea.offset.x - static_cast<int>(m_parameters.renderSize.x() - renderArea.extent.width) / 2) * pixelSizeX;
		const float				offsetY								= static_cast<float>(renderArea.offset.y - static_cast<int>(m_parameters.renderSize.y() - renderArea.extent.height) / 2) * pixelSizeY;
		const Vec4				offset								= {offsetX, offsetY, 0.0f, 0.0f};
		const deUint32			quadMarginX							= renderWidth / 4 + quadShrink;
		const deUint32			quadMarginY							= renderHeight / 4 + quadShrink;
		const deUint32			quadTop								= renderY + quadMarginY;
		const deUint32			quadBottom							= renderY + renderHeight - quadMarginY;
		const deUint32			quadLeft							= renderX + quadMarginX;
		const deUint32			quadRight							= renderX + renderWidth - quadMarginX;

		const Vec4				color								= {m_random.getFloat(), m_random.getFloat(), m_random.getFloat(), 1.0f};
		const deInt32			layer								= useLayers ? m_random.getUint32() % numLayers : 0;

		PushConstantData		pcd									= {scale, offset, color, layer};

		// Bind vertex buffer.
		{
			const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
			const VkDeviceSize	vertexBufferOffset	= 0ull;

			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		if (occlusionQuery)
			vk.cmdBeginQuery(cmdBuffer, *m_queryPool, queryIndex, 0);

		vk.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(pcd), &pcd);

		// Draw or clear a quad inside the render area.
		switch (pipelineType)
		{
			case PIPELINE_TYPE_VERTEX_FRAGMENT: // A quad using triangle strip.
			case PIPELINE_TYPE_VERTEX_TESS_FRAGMENT: // A quad using a tessellated patch.
			case PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT_LAYER: // A quad using triangle strip drawn to a chosen layer.
			case PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW: // A quad using triangle strip drawn to layers 0, 1, and 3.
			{
				vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
			}
			break;
			case PIPELINE_TYPE_VERTEX_GEOM_FRAGMENT: // A single point turned into a quad by geometry shader.
			{
				vk.cmdDraw(cmdBuffer, 1u, 1u, 0u, 0u);
			}
			break;
			case PIPELINE_TYPE_ATTACHMENT_CLEAR: // A quad using vkCmdClearAttachments
			{
				VkClearAttachment	clearAttachment	= { VK_IMAGE_ASPECT_COLOR_BIT, 0u, makeClearValueColor(color) };
				VkClearRect			rect			= { makeRect2D(quadLeft, quadTop, quadRight - quadLeft, quadBottom - quadTop), static_cast<deUint32>(layer), 1u };

				vk.cmdClearAttachments(cmdBuffer, 1u, &clearAttachment, 1u, &rect);
			}
			break;
			default:
			{
				DE_ASSERT(0 && "Unexpected pipeline type.");
			}
			break;
		}

		if (occlusionQuery)
		{
			vk.cmdEndQuery(cmdBuffer, *m_queryPool, queryIndex);
			if (pipelineType == PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW) {
				queryIndex += 3;
				queryMultiviewCount.push_back(3);
				queryMultiviewCount.push_back(0);
				queryMultiviewCount.push_back(0);
			}
			else
			{
				queryIndex++;
				queryMultiviewCount.push_back(1);
			}
		}

		deUint32				activeLayersClear					= 0x1;
		deUint32				activeLayersQuad					= 0x1;

		if (useLayers)
		{
			activeLayersClear	= (1 << numLayers) - 1;
			activeLayersQuad	= 1 << layer;
		}
		else if (pipelineType == PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW)
		{
			activeLayersClear = activeLayersQuad = viewMask;
		}

		// Update reference image.
		for (deUint32 l = 0; l < numLayers; l++)
		{
			tcu::PixelBufferAccess access = ref[l].getAccess();
			for (deUint32 y = renderY; y < renderY + renderHeight; y++)
				for (deUint32 x = renderX; x < renderX + renderWidth; x++)
				{
					if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && !(flags & VK_RENDERING_RESUMING_BIT_KHR) && (activeLayersClear & (1 <<l)))
					{
						access.setPixel(clearColor, x, y);
					}

					if (x >= quadLeft && x < quadRight && y >= quadTop && y < quadBottom && (activeLayersQuad & (1 << l)))
					{
						// Inside the drawn quad.
						Vec4 refColor = color;

						if (pipelineType == PIPELINE_TYPE_VERTEX_FRAGMENT_MULTIVIEW)
							refColor.z() = 0.15f * static_cast<float>(l);
						else if (pipelineType != PIPELINE_TYPE_ATTACHMENT_CLEAR)
							refColor.z() = 0.0f;

						access.setPixel(refColor, x, y);
					}
				}
		}

		if (useSecondaryCmdBuffer)
		{
			// End the secondary buffer
			VK_CHECK(vk.endCommandBuffer(cmdBuffer));

			// Call the secondary buffer
			vk.cmdExecuteCommands(*cmdBuffers.back(), 1u, &cmdBuffer);
		}

		vk.cmdEndRendering(*cmdBuffers.back());

		// Insert a pipeline barrier if not suspending.
		if (!suspend)
		{
			VkMemoryBarrier	barrier	=
			{
					VK_STRUCTURE_TYPE_MEMORY_BARRIER,		// VkStructureType	sType
					DE_NULL,								// const void*		pNext
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,	// VkAccessFlags	srcAccessMask
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT	// VkAccessFlags	dstAccessMask
			};

			vk.cmdPipelineBarrier(*cmdBuffers.back(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
								  1u, &barrier, 0u, DE_NULL, 0u, DE_NULL);
		}

		prevFlags = flags;
	}

	vk.cmdCopyQueryPoolResults(*cmdBuffers.back(), *m_queryPool, 0, queryIndex, m_queryResults->object(), 0, sizeof(deUint32), VK_QUERY_RESULT_WAIT_BIT);
	copyImageToBuffer(vk, *cmdBuffers.back(), *m_imageColor, m_imageBuffer->object(),
					  tcu::IVec2(m_parameters.renderSize.x(), m_parameters.renderSize.y()),
					  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, numLayers,
					  VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffers.back()));

	// Submit commands and wait.
	{
		const Unique<VkFence>			fence				(createFence(vk, device));
		std::vector<VkCommandBuffer>	cmdBufferHandles;

		for (const auto& cmdBuffer : cmdBuffers)
			cmdBufferHandles.push_back(*cmdBuffer);

		const VkSubmitInfo				submitInfo			=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,					// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			0u,												// deUint32						waitSemaphoreCount
			DE_NULL,										// const VkSemaphore*			pWaitSemaphores
			DE_NULL,										// const VkPipelineStageFlags*	pWaitDstStageMask
			static_cast<deUint32>(cmdBufferHandles.size()),	// deUint32						commandBufferCount
			cmdBufferHandles.data(),						// const VkCommandBuffer*		pCommandBuffers
			0u,												// deUint32						signalSemaphoreCount
			DE_NULL,										// const VkSemaphore*			pSignalSemaphores
		};

		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// Verify result image.
	for (deUint32 i = 0; i < numLayers; i++)
	{
		const Allocation					allocColor			= m_imageBuffer->getBoundMemory();
		invalidateAlloc(vk, device, allocColor);

		const tcu::ConstPixelBufferAccess	resultColorImage	(mapVkFormat(m_parameters.imageFormat), m_parameters.renderSize.x(),
																 m_parameters.renderSize.y(), 1u, static_cast<deUint8*>(allocColor.getHostPtr()) + m_layerSizeBytes * i);

		if (!tcu::floatThresholdCompare(log, "Compare Color Image", "Result comparison", ref[i].getAccess(), resultColorImage, Vec4(0.02f), tcu::COMPARE_LOG_ON_ERROR))
		{
			return tcu::TestStatus::fail("Rendered color image is not correct");
		}
	}

	// Verify query pool results.
	{
		deUint32* queryPtr = static_cast<deUint32*>(m_queryResults->getBoundMemory().getHostPtr());
		invalidateAlloc(vk, device, m_queryResults->getBoundMemory());

		deUint32 i = 0;
		while (i < queryIndex)
		{
			uint32_t querySum = 0;
			for (uint32_t j = 0; j < queryMultiviewCount[i]; j++) {
				querySum += queryPtr[i];
			}
			if (querySum == 0)
			{
				return tcu::TestStatus::fail("Expected nonzero occlusion query results.");
			}
			i += queryMultiviewCount[i];
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class RandomTestCase : public TestCase
{
public:
							RandomTestCase	(tcu::TestContext&		context,
											 const std::string&		name,
											 const std::string&		description,
											 const TestParameters&	parameters);
	virtual					~RandomTestCase	(void);

protected:
	virtual void			checkSupport	(Context&				context) const;
	virtual void			initPrograms	(SourceCollections&		programCollection) const;
	virtual TestInstance*	createInstance	(Context&				context) const;

	const TestParameters	m_parameters;
};

RandomTestCase::RandomTestCase (tcu::TestContext&		context,
								const std::string&		name,
								const std::string&		description,
								const TestParameters&	parameters)
: TestCase		(context, name, description)
, m_parameters	(parameters)
{
}

RandomTestCase::~RandomTestCase ()
{
}

void RandomTestCase::checkSupport (Context& context) const
{
	if (!context.requireDeviceFunctionality("VK_KHR_dynamic_rendering"))
		TCU_THROW(NotSupportedError, "VK_KHR_dynamic_rendering is not supported");

	const VkPhysicalDeviceDynamicRenderingFeaturesKHR& dynamicRenderingFeatures(context.getDynamicRenderingFeatures());

	if (dynamicRenderingFeatures.dynamicRendering == DE_FALSE)
		TCU_THROW(NotSupportedError, "dynamicRendering feature is not supported");

	if (m_parameters.enableGeometry)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	if (m_parameters.enableTessellation)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

void RandomTestCase::initPrograms (SourceCollections& programCollection) const
{
	const std::string pushConstant =
		"layout( push_constant ) uniform constants\n"
		"{\n"
		"	vec4 scale;\n"
		"	vec4 offset;\n"
		"	vec4 color;\n"
		"	int layer;\n"
		"} pc;\n";

	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 position;\n"
			<< "layout(location = 0) out vec4 vsColor;\n"
			<< "\n"
			<< pushConstant
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position * pc.scale + pc.offset;\n"
			<< "    vsColor     = pc.color;\n"
			<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Passthrough vertex
    if (m_parameters.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 position;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";
		programCollection.glslSources.add("vertPassthrough") << glu::VertexSource(src.str());
	}

	// Vertex layered
	if (m_parameters.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout(location = 0) in vec4 position;\n"
			<< "layout(location = 0) out vec4 positionOut;\n"
			<< "\n"
			<< pushConstant
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    positionOut = position * pc.scale + pc.offset;\n"
			<< "}\n";
		programCollection.glslSources.add("vertLayer") << glu::VertexSource(src.str());
	}

	// Geometry
	if (m_parameters.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(points) in;\n"
			<< "layout(triangle_strip, max_vertices = 4) out;\n"
			<< "layout(location = 0) out vec4 vsColor;\n"
			<< "\n"
			<< pushConstant
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    vec4 quad[4] = vec4[4](vec4(-0.5, 0.5, 0, 1), vec4(-0.5, -0.5, 0, 1), vec4(0.5, 0.5, 0, 1), vec4(0.5, -0.5, 0, 1));\n"
			<< "    for (int i = 0; i < 4; i++)\n"
			<< "    {\n"
			<< "        gl_Position = quad[i] * pc.scale + pc.offset;\n"
			<< "        vsColor     = pc.color;\n"
			<< "        EmitVertex();\n"
			<< "    }\n"
			<< "    EndPrimitive();\n"
			<< "}\n";
		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Geometry passthrough with layer
	if (m_parameters.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = 3) out;\n"
			<< "layout(location = 0) in vec4 position[];\n"
			<< "layout(location = 0) out vec4 vsColor;\n"
			<< "\n"
			<< pushConstant
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    for (int i = 0; i < 3; i++)\n"
			<< "    {\n"
			<< "        gl_Position = position[i];\n"
			<< "        vsColor     = pc.color;\n"
			<< "        gl_Layer    = pc.layer;\n"
			<< "        EmitVertex();\n"
			<< "    }\n"
			<< "    EndPrimitive();\n"
			<< "}\n";
		programCollection.glslSources.add("geomLayer") << glu::GeometrySource(src.str());
	}

	// Tessellation control
	if (m_parameters.enableTessellation)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(vertices = 4) out;\n"
			<< "layout(location = 0) in vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (gl_InvocationID == 0)\n"
			<< "    {\n"
			<< "        gl_TessLevelInner[0] = 2.0f;\n"
			<< "        gl_TessLevelInner[1] = 2.0f;\n"
			<< "        gl_TessLevelOuter[0] = 2.0f;\n"
			<< "        gl_TessLevelOuter[1] = 2.0f;\n"
			<< "        gl_TessLevelOuter[2] = 2.0f;\n"
			<< "        gl_TessLevelOuter[3] = 2.0f;\n"
			<< "    }\n"
			<< "    out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";
		programCollection.glslSources.add("tsc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation
	if (m_parameters.enableTessellation)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(quads, equal_spacing, ccw) in;\n"
			<< "layout(location = 0) in vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    const float u = gl_TessCoord.x;\n"
			<< "    const float v = gl_TessCoord.y;\n"
			<< "    gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position + (1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n"
			<< "    out_color = in_color[0];\n"
			<< "}\n";
		programCollection.glslSources.add("tse") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_multiview : require\n"
			<< "\n"
			<< "layout(location = 0) in vec4 vsColor;\n"
			<< "layout(location = 0) out vec4 fsColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    fsColor   = vsColor;\n"
			<< "    fsColor.z = 0.15f * gl_ViewIndex;\n"
			<< "}\n";
		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance*	RandomTestCase::createInstance (Context& context) const
{
	return new DynamicRenderingTestInstance(context, m_parameters);
}

tcu::TestNode* addDynamicRenderingTest (tcu::TestContext& testCtx, TestParameters& parameters)
{
	const std::string testName = "seed" + de::toString(parameters.randomSeed) + (parameters.enableGeometry ? "_geometry" : "") + (parameters.enableTessellation ? "_tessellation" : "");

	return new RandomTestCase(testCtx, testName, "", parameters);
}

} // anonymous

tcu::TestCaseGroup* createDynamicRenderingRandomTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup (new tcu::TestCaseGroup(testCtx, "random", "Random dynamic rendering tests"));

	for (auto geometry : {true, false})
		for (auto tessellation : {true, false})
		{
			TestParameters parameters =
			{
				VK_FORMAT_R8G8B8A8_UNORM,
				(UVec2(256, 256)),
				geometry,
				tessellation,
				0u
			};

			for (deUint32 i = 0; i < 100u; i++)
			{
				parameters.randomSeed = i;
				dynamicRenderingGroup->addChild(addDynamicRenderingTest(testCtx, parameters));
			}
		}

	return dynamicRenderingGroup.release();
}

} // renderpass
} // vkt
