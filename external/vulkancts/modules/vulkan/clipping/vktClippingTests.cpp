/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Clipping tests
 *//*--------------------------------------------------------------------*/

#include "vktClippingTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktClippingUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTestLog.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace clipping
{
namespace
{
using namespace vk;
using de::MovePtr;
using tcu::UVec2;
using tcu::Vec4;
using tcu::IVec2;

enum Constants
{
	RENDER_SIZE								= 16,
	RENDER_SIZE_LARGE						= 128,
	NUM_RENDER_PIXELS						= RENDER_SIZE * RENDER_SIZE,
	NUM_PATCH_CONTROL_POINTS				= 3,
	MAX_NUM_SHADER_MODULES					= 5,
	MAX_CLIP_DISTANCES						= 8,
	MAX_CULL_DISTANCES						= 8,
	MAX_COMBINED_CLIP_AND_CULL_DISTANCES	= 8,
};

struct Shader
{
	VkShaderStageFlagBits	stage;
	const ProgramBinary*	binary;

	Shader (const VkShaderStageFlagBits stage_, const ProgramBinary& binary_)
		: stage		(stage_)
		, binary	(&binary_)
	{
	}
};

//! Sets up a graphics pipeline and enables simple draw calls to predefined attachments.
//! Clip volume uses wc = 1.0, which gives clip coord ranges: x = [-1, 1], y = [-1, 1], z = [0, 1]
//! Clip coords (-1,-1) map to viewport coords (0, 0).
class DrawContext
{
public:
									DrawContext		(Context&						context,
													 const std::vector<Shader>&		shaders,
													 const std::vector<Vec4>&		vertices,
													 const VkPrimitiveTopology		primitiveTopology,
													 const deUint32					renderSize			= static_cast<deUint32>(RENDER_SIZE),
													 const bool						depthClampEnable	= false,
													 const bool						blendEnable			= false,
													 const float					lineWidth			= 1.0f);

	void							draw			(void);
	tcu::ConstPixelBufferAccess		getColorPixels	(void) const;

private:
	Context&						m_context;
	const VkFormat					m_colorFormat;
	const VkImageSubresourceRange	m_colorSubresourceRange;
	const UVec2						m_renderSize;
	const VkExtent3D				m_imageExtent;
	const VkPrimitiveTopology		m_primitiveTopology;
	const bool						m_depthClampEnable;
	const bool						m_blendEnable;
	const deUint32					m_numVertices;
	const float						m_lineWidth;
	const deUint32					m_numPatchControlPoints;
	MovePtr<Buffer>					m_vertexBuffer;
	MovePtr<Image>					m_colorImage;
	MovePtr<Buffer>					m_colorAttachmentBuffer;
	Move<VkImageView>				m_colorImageView;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_pipeline;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	Move<VkShaderModule>			m_shaderModules[MAX_NUM_SHADER_MODULES];

									DrawContext		(const DrawContext&);	// "deleted"
	DrawContext&					operator=		(const DrawContext&);	// "deleted"
};

DrawContext::DrawContext (Context&						context,
						  const std::vector<Shader>&	shaders,
						  const std::vector<Vec4>&		vertices,
						  const VkPrimitiveTopology		primitiveTopology,
						  const deUint32				renderSize,
						  const bool					depthClampEnable,
						  const bool					blendEnable,
						  const float					lineWidth)
	: m_context					(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_colorSubresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
	, m_renderSize				(renderSize, renderSize)
	, m_imageExtent				(makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u))
	, m_primitiveTopology		(primitiveTopology)
	, m_depthClampEnable		(depthClampEnable)
	, m_blendEnable				(blendEnable)
	, m_numVertices				(static_cast<deUint32>(vertices.size()))
	, m_lineWidth				(lineWidth)
	, m_numPatchControlPoints	(NUM_PATCH_CONTROL_POINTS)		// we're treating patches as triangles
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// Command buffer
	{
		m_cmdPool	= makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
		m_cmdBuffer	= makeCommandBuffer(vk, device, *m_cmdPool);
	}

	// Color attachment image
	{
		const VkImageUsageFlags usage			= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,									// const void*              pNext;
			(VkImageCreateFlags)0,						// VkImageCreateFlags       flags;
			VK_IMAGE_TYPE_2D,							// VkImageType              imageType;
			m_colorFormat,								// VkFormat                 format;
			m_imageExtent,								// VkExtent3D               extent;
			1u,											// uint32_t                 mipLevels;
			1u,											// uint32_t                 arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits    samples;
			VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling            tiling;
			usage,										// VkImageUsageFlags        usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t                 queueFamilyIndexCount;
			DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
		};

		m_colorImage = MovePtr<Image>(new Image(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
		m_colorImageView = makeImageView(vk, device, **m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorSubresourceRange);

		// Buffer to copy attachment data after rendering

		const VkDeviceSize bitmapSize = tcu::getPixelSize(mapVkFormat(m_colorFormat)) * m_renderSize.x() * m_renderSize.y();
		m_colorAttachmentBuffer = MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(bitmapSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

		{
			const Allocation& alloc = m_colorAttachmentBuffer->getAllocation();
			deMemset(alloc.getHostPtr(), 0, (size_t)bitmapSize);
			flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), bitmapSize);
		}
	}

	// Vertex buffer
	{
		const VkDeviceSize bufferSize = vertices.size() * sizeof(vertices[0]);
		m_vertexBuffer = MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_vertexBuffer->getAllocation();
		deMemcpy(alloc.getHostPtr(), &vertices[0], (size_t)bufferSize);
		flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), bufferSize);
	}

	// Pipeline layout
	{
		m_pipelineLayout = makePipelineLayoutWithoutDescriptors(vk, device);
	}

	// Renderpass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
			m_colorFormat,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					finalLayout;
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
			VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
			0u,													// deUint32							inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
			1u,													// deUint32							colorAttachmentCount;
			&colorAttachmentReference,							// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
			&depthAttachmentReference,							// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,													// deUint32							preserveAttachmentCount;
			DE_NULL												// const deUint32*					pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
			1u,													// deUint32							attachmentCount;
			&colorAttachmentDescription,						// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, device, &renderPassInfo);
	}

	// Framebuffer
	{
		const VkFramebufferCreateInfo framebufferInfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
			DE_NULL,										// const void*                                 pNext;
			(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
			*m_renderPass,									// VkRenderPass                                renderPass;
			1u,												// uint32_t                                    attachmentCount;
			&m_colorImageView.get(),						// const VkImageView*                          pAttachments;
			m_renderSize.x(),								// uint32_t                                    width;
			m_renderSize.y(),								// uint32_t                                    height;
			1u,												// uint32_t                                    layers;
		};

		m_framebuffer = createFramebuffer(vk, device, &framebufferInfo);
	}

	// Graphics pipeline
	{
		const deUint32	vertexStride	= sizeof(Vec4);
		const VkFormat	vertexFormat	= VK_FORMAT_R32G32B32A32_SFLOAT;

		const VkVertexInputBindingDescription bindingDesc =
		{
			0u,									// uint32_t				binding;
			vertexStride,						// uint32_t				stride;
			VK_VERTEX_INPUT_RATE_VERTEX,		// VkVertexInputRate	inputRate;
		};
		const VkVertexInputAttributeDescription attributeDesc =
		{
			0u,									// uint32_t			location;
			0u,									// uint32_t			binding;
			vertexFormat,						// VkFormat			format;
			0u,									// uint32_t			offset;
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
			DE_NULL,														// const void*                                 pNext;
			(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
			1u,																// uint32_t                                    vertexBindingDescriptionCount;
			&bindingDesc,													// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
			1u,																// uint32_t                                    vertexAttributeDescriptionCount;
			&attributeDesc,													// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
			DE_NULL,														// const void*                                 pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags     flags;
			m_primitiveTopology,											// VkPrimitiveTopology                         topology;
			VK_FALSE,														// VkBool32                                    primitiveRestartEnable;
		};

		const VkPipelineTessellationStateCreateInfo pipelineTessellationStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType                             sType;
			DE_NULL,														// const void*                                 pNext;
			(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags      flags;
			m_numPatchControlPoints,										// uint32_t                                    patchControlPoints;
		};

		const VkViewport viewport = makeViewport(
			0.0f, 0.0f,
			static_cast<float>(m_renderSize.x()), static_cast<float>(m_renderSize.y()),
			0.0f, 1.0f);

		const VkRect2D scissor = {
			makeOffset2D(0, 0),
			makeExtent2D(m_renderSize.x(), m_renderSize.y()),
		};

		const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType;
			DE_NULL,												// const void*                                 pNext;
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags;
			1u,														// uint32_t                                    viewportCount;
			&viewport,												// const VkViewport*                           pViewports;
			1u,														// uint32_t                                    scissorCount;
			&scissor,												// const VkRect2D*                             pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
			DE_NULL,														// const void*                              pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
			m_depthClampEnable,												// VkBool32                                 depthClampEnable;
			VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			m_lineWidth,													// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,													// VkBool32									sampleShadingEnable;
			0.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			VK_FALSE,													// VkBool32									alphaToCoverageEnable;
			VK_FALSE													// VkBool32									alphaToOneEnable;
		};

		const VkStencilOpState stencilOpState = makeStencilOpState(
			VK_STENCIL_OP_KEEP,		// stencil fail
			VK_STENCIL_OP_KEEP,		// depth & stencil pass
			VK_STENCIL_OP_KEEP,		// depth only fail
			VK_COMPARE_OP_NEVER,	// compare op
			0u,						// compare mask
			0u,						// write mask
			0u);					// reference

		const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
			VK_FALSE,													// VkBool32									depthTestEnable;
			VK_FALSE,													// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			VK_FALSE,													// VkBool32									depthBoundsTestEnable;
			VK_FALSE,													// VkBool32									stencilTestEnable;
			stencilOpState,												// VkStencilOpState							front;
			stencilOpState,												// VkStencilOpState							back;
			0.0f,														// float									minDepthBounds;
			1.0f,														// float									maxDepthBounds;
		};

		const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
		{
			m_blendEnable,						// VkBool32					blendEnable;
			VK_BLEND_FACTOR_SRC_ALPHA,			// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ONE,				// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,					// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_SRC_ALPHA,			// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ONE,				// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,					// VkBlendOp				alphaBlendOp;
			colorComponentsAll,					// VkColorComponentFlags	colorWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
			VK_FALSE,													// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&pipelineColorBlendAttachmentState,							// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
		};

		// Create shader stages

		std::vector<VkPipelineShaderStageCreateInfo>	shaderStages;
		VkShaderStageFlags								stageFlags = (VkShaderStageFlags)0;

		DE_ASSERT(shaders.size() <= MAX_NUM_SHADER_MODULES);
		for (deUint32 shaderNdx = 0; shaderNdx < shaders.size(); ++shaderNdx)
		{
			m_shaderModules[shaderNdx] = createShaderModule(vk, device, *shaders[shaderNdx].binary, (VkShaderModuleCreateFlags)0);

			const VkPipelineShaderStageCreateInfo pipelineShaderStageInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
				shaders[shaderNdx].stage,								// VkShaderStageFlagBits				stage;
				*m_shaderModules[shaderNdx],							// VkShaderModule						module;
				"main",													// const char*							pName;
				DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
			};

			shaderStages.push_back(pipelineShaderStageInfo);
			stageFlags |= shaders[shaderNdx].stage;
		}

		DE_ASSERT(
			(m_primitiveTopology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) ||
			(stageFlags & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)));

		const bool tessellationEnabled = (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
		const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,						// VkStructureType									sType;
			DE_NULL,																// const void*										pNext;
			(VkPipelineCreateFlags)0,												// VkPipelineCreateFlags							flags;
			static_cast<deUint32>(shaderStages.size()),								// deUint32											stageCount;
			&shaderStages[0],														// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateInfo,													// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&pipelineInputAssemblyStateInfo,										// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			(tessellationEnabled ? &pipelineTessellationStateInfo : DE_NULL),		// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&pipelineViewportStateInfo,												// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&pipelineRasterizationStateInfo,										// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&pipelineMultisampleStateInfo,											// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&pipelineDepthStencilStateInfo,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&pipelineColorBlendStateInfo,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,																// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*m_pipelineLayout,														// VkPipelineLayout									layout;
			*m_renderPass,															// VkRenderPass										renderPass;
			0u,																		// deUint32											subpass;
			DE_NULL,																// VkPipeline										basePipelineHandle;
			0,																		// deInt32											basePipelineIndex;
		};

		m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
	}

	// Record commands
	{
		const VkDeviceSize zeroOffset = 0ull;

		beginCommandBuffer(vk, *m_cmdBuffer);

		// Begin render pass
		{
			const VkClearValue	clearValue = makeClearValueColor(Vec4(0.0f, 0.0f, 0.0f, 1.0f));
			const VkRect2D		renderArea =
			{
				makeOffset2D(0, 0),
				makeExtent2D(m_renderSize.x(), m_renderSize.y())
			};

			const VkRenderPassBeginInfo renderPassBeginInfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType         sType;
				DE_NULL,										// const void*             pNext;
				*m_renderPass,									// VkRenderPass            renderPass;
				*m_framebuffer,									// VkFramebuffer           framebuffer;
				renderArea,										// VkRect2D                renderArea;
				1u,												// uint32_t                clearValueCount;
				&clearValue,									// const VkClearValue*     pClearValues;
			};

			vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(**m_vertexBuffer), &zeroOffset);

		vk.cmdDraw(*m_cmdBuffer, m_numVertices, 1u, 0u, 1u);
		vk.cmdEndRenderPass(*m_cmdBuffer);

		// Barrier: draw -> copy from image
		{
			const VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				**m_colorImage, m_colorSubresourceRange);

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
				0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
		}

		{
			const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u), m_imageExtent);
			vk.cmdCopyImageToBuffer(*m_cmdBuffer, **m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_colorAttachmentBuffer, 1u, &copyRegion);
		}

		// Barrier: copy to buffer -> host read
		{
			const VkBufferMemoryBarrier barrier = makeBufferMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
				**m_colorAttachmentBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
				0u, DE_NULL, 1u, &barrier, 0u, DE_NULL);
		}

		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

void DrawContext::draw (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	const VkQueue			queue		= m_context.getUniversalQueue();
	tcu::TestLog&			log			= m_context.getTestContext().getLog();

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	log << tcu::LogImageSet("attachments", "") << tcu::LogImage("color0", "", getColorPixels()) << tcu::TestLog::EndImageSet;
}

tcu::ConstPixelBufferAccess DrawContext::getColorPixels (void) const
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	const Allocation& alloc = m_colorAttachmentBuffer->getAllocation();
	invalidateMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), VK_WHOLE_SIZE);

	return tcu::ConstPixelBufferAccess(mapVkFormat(m_colorFormat), m_imageExtent.width, m_imageExtent.height, m_imageExtent.depth, alloc.getHostPtr());
}

std::vector<Vec4> genVertices (const VkPrimitiveTopology topology, const Vec4& offset, const float slope)
{
	const float p  = 1.0f;
	const float hp = 0.5f;
	const float z  = 0.0f;
	const float w  = 1.0f;

	std::vector<Vec4> vertices;

	// We're setting adjacent vertices to zero where needed, as we don't use them in meaningful way.

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			vertices.push_back(offset + Vec4(0.0f, 0.0f, slope/2.0f + z, w));
			vertices.push_back(offset + Vec4( -hp,  -hp,              z, w));
			vertices.push_back(offset + Vec4(  hp,  -hp,      slope + z, w));
			vertices.push_back(offset + Vec4( -hp,   hp,              z, w));
			vertices.push_back(offset + Vec4(  hp,   hp,      slope + z, w));
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(Vec4());
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(Vec4());
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 1
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 0
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
			DE_ASSERT(0);
			break;

		default:
			DE_ASSERT(0);
			break;
	}
	return vertices;
}

bool inline isColorInRange (const Vec4& color, const Vec4& minColor, const Vec4& maxColor)
{
	return (minColor.x() <= color.x() && color.x() <= maxColor.x())
		&& (minColor.y() <= color.y() && color.y() <= maxColor.y())
		&& (minColor.z() <= color.z() && color.z() <= maxColor.z())
		&& (minColor.w() <= color.w() && color.w() <= maxColor.w());
}

//! Count pixels that match color within threshold, in the specified region.
int countPixels (const tcu::ConstPixelBufferAccess pixels, const IVec2& regionOffset, const IVec2& regionSize, const Vec4& color, const Vec4& colorThreshold)
{
	const Vec4	minColor	= color - colorThreshold;
	const Vec4	maxColor	= color + colorThreshold;
	const int	xEnd		= regionOffset.x() + regionSize.x();
	const int	yEnd		= regionOffset.y() + regionSize.y();
	int			numPixels	= 0;

	DE_ASSERT(xEnd <= pixels.getWidth());
	DE_ASSERT(yEnd <= pixels.getHeight());

	for (int y = regionOffset.y(); y < yEnd; ++y)
	for (int x = regionOffset.x(); x < xEnd; ++x)
	{
		if (isColorInRange(pixels.getPixel(x, y), minColor, maxColor))
			++numPixels;
	}

	return numPixels;
}

int countPixels (const tcu::ConstPixelBufferAccess pixels, const Vec4& color, const Vec4& colorThreshold)
{
	return countPixels(pixels, IVec2(), IVec2(pixels.getWidth(), pixels.getHeight()), color, colorThreshold);
}

//! Clipping against the default clip volume.
namespace ClipVolume
{

//! Used by wide lines test.
enum LineOrientation
{
	LINE_ORIENTATION_AXIS_ALIGNED,
	LINE_ORIENTATION_DIAGONAL,
};

const VkPointClippingBehaviorKHR invalidClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_KHR_LAST;

VkPointClippingBehaviorKHR getClippingBehavior (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDevicePointClippingPropertiesKHR	behaviorProperties	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES_KHR,	// VkStructureType				sType
		DE_NULL,															// void*						pNext
		invalidClippingBehavior												// VkPointClippingBehaviorKHR	pointClippingBehavior
	};
	VkPhysicalDeviceProperties2KHR				properties2;

	DE_ASSERT(getPointClippingBehaviorKHRName(invalidClippingBehavior) == DE_NULL);

	deMemset(&properties2, 0, sizeof(properties2));

	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	properties2.pNext = &behaviorProperties;

	vk.getPhysicalDeviceProperties2KHR(physicalDevice, &properties2);

	return behaviorProperties.pointClippingBehavior;
}

void addSimplePrograms (SourceCollections& programCollection, const float pointSize = 0.0f)
{
	// Vertex shader
	{
		const bool usePointSize = pointSize > 0.0f;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 v_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n"
			<< (usePointSize ? "    float gl_PointSize;\n" : "")
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = v_position;\n"
			<< (usePointSize ? "    gl_PointSize = " + de::floatToString(pointSize, 1) + ";\n" : "")
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, gl_FragCoord.z, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initPrograms (SourceCollections& programCollection, const VkPrimitiveTopology topology)
{
	const float pointSize = (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? 1.0f : 0.0f);
	addSimplePrograms(programCollection, pointSize);
}

void initPrograms (SourceCollections& programCollection, const LineOrientation lineOrientation)
{
	DE_UNREF(lineOrientation);
	addSimplePrograms(programCollection);
}

void initProgramsPointSize (SourceCollections& programCollection)
{
	addSimplePrograms(programCollection, 0.75f * RENDER_SIZE);
}

//! Primitives fully inside the clip volume.
tcu::TestStatus testPrimitivesInside (Context& context, const VkPrimitiveTopology topology)
{
	int minExpectedBlackPixels = 0;

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			// We draw only 5 points.
			minExpectedBlackPixels = NUM_RENDER_PIXELS - 5;
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			// Allow for some error.
			minExpectedBlackPixels = NUM_RENDER_PIXELS - 3 * RENDER_SIZE;
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			// All render area should be covered.
			minExpectedBlackPixels = 0;
			break;

		default:
			DE_ASSERT(0);
			break;
	}

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	tcu::TestLog&	log			= context.getTestContext().getLog();
	int				numPassed	= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
	} cases[] =
	{
		{ "Draw primitives at near clipping plane, z = 0.0",	0.0f, },
		{ "Draw primitives at z = 0.5",							0.5f, },
		{ "Draw primitives at far clipping plane, z = 1.0",		1.0f, },
	};

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4> vertices = genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 0.0f);
		DrawContext drawContext(context, shaders, vertices, topology);
		drawContext.draw();

		const int numBlackPixels = countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
		if (numBlackPixels >= minExpectedBlackPixels)
			++numPassed;
	}

	return (numPassed == DE_LENGTH_OF_ARRAY(cases) ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Primitives fully outside the clip volume.
tcu::TestStatus testPrimitivesOutside (Context& context, const VkPrimitiveTopology topology)
{
	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	tcu::TestLog&	log			= context.getTestContext().getLog();
	int				numPassed	= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
	} cases[] =
	{
		{ "Draw primitives in front of the near clipping plane, z < 0.0",	-0.5f, },
		{ "Draw primitives behind the far clipping plane, z > 1.0",			 1.5f, },
	};

	log << tcu::TestLog::Message << "Drawing primitives outside the clip volume. Expecting an empty image." << tcu::TestLog::EndMessage;

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4> vertices = genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 0.0f);
		DrawContext drawContext(context, shaders, vertices, topology);
		drawContext.draw();

		// All pixels must be black -- nothing is drawn.
		const int numBlackPixels = countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
		if (numBlackPixels == NUM_RENDER_PIXELS)
			++numPassed;
	}

	return (numPassed == DE_LENGTH_OF_ARRAY(cases) ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Primitives partially outside the clip volume, but depth clamped
tcu::TestStatus testPrimitivesDepthClamp (Context& context, const VkPrimitiveTopology topology)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_DEPTH_CLAMP);

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	const int		numCases		= 4;
	const IVec2		regionSize		= IVec2(RENDER_SIZE/2, RENDER_SIZE);	//! size of the clamped region
	const int		regionPixels	= regionSize.x() * regionSize.y();
	tcu::TestLog&	log				= context.getTestContext().getLog();
	int				numPassed		= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
		bool				depthClampEnable;
		IVec2				regionOffset;
		Vec4				color;
	} cases[numCases] =
	{
		{ "Draw primitives intersecting the near clipping plane, depth clamp disabled",	-0.5f,	false,	IVec2(0, 0),				Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the near clipping plane, depth clamp enabled",	-0.5f,	true,	IVec2(0, 0),				Vec4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clamp disabled",	 0.5f,	false,	IVec2(RENDER_SIZE/2, 0),	Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clamp enabled",	 0.5f,	true,	IVec2(RENDER_SIZE/2, 0),	Vec4(1.0f, 1.0f, 0.0f, 1.0f) },
	};

	// Per case minimum number of colored pixels.
	int caseMinPixels[numCases] = { 0, 0, 0, 0 };

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			caseMinPixels[0] = caseMinPixels[2] = regionPixels - 1;
			caseMinPixels[1] = caseMinPixels[3] = 2;
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			caseMinPixels[0] = regionPixels;
			caseMinPixels[1] = RENDER_SIZE - 2;
			caseMinPixels[2] = regionPixels;
			caseMinPixels[3] = 2 * (RENDER_SIZE - 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			caseMinPixels[0] = caseMinPixels[1] = caseMinPixels[2] = caseMinPixels[3] = regionPixels;
			break;

		default:
			DE_ASSERT(0);
			break;
	}

	for (int caseNdx = 0; caseNdx < numCases; ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4> vertices = genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 1.0f);
		DrawContext drawContext(context, shaders, vertices, topology, static_cast<deUint32>(RENDER_SIZE), cases[caseNdx].depthClampEnable);
		drawContext.draw();

		const int numPixels = countPixels(drawContext.getColorPixels(), cases[caseNdx].regionOffset, regionSize, cases[caseNdx].color, Vec4());

		if (numPixels >= caseMinPixels[caseNdx])
			++numPassed;
	}

	return (numPassed == numCases ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Large point clipping
//! Spec: If the primitive under consideration is a point, then clipping passes it unchanged if it lies within the clip volume;
//!       otherwise, it is discarded.
tcu::TestStatus testLargePoints (Context& context)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_LARGE_POINTS);

	bool pointClippingOutside = true;

	if (de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_KHR_maintenance2"))
	{
		VkPointClippingBehaviorKHR clippingBehavior = getClippingBehavior(context.getInstanceInterface(), context.getPhysicalDevice());

		switch (clippingBehavior)
		{
			case VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES_KHR:		pointClippingOutside = true;				break;
			case VK_POINT_CLIPPING_BEHAVIOR_USER_CLIP_PLANES_ONLY_KHR:	pointClippingOutside = false;				break;
			case invalidClippingBehavior:								TCU_FAIL("Clipping behavior read failure");	break;
			default:
			{
				TCU_FAIL("Unexpected clipping behavior reported");
			}
		}
	}
	else
	{
		//TODO: Now we have 2 cases {some-points-drawn|nothing}, we should have {all-points-drawn|some-points-drawn|nothing}
		return tcu::TestStatus::pass("OK");
	}

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	std::vector<Vec4> vertices;
	{
		const float delta	= 0.1f;  // much smaller than the point size
		const float p		= 1.0f + delta;

		vertices.push_back(Vec4(  -p,   -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(  -p,    p, 0.2f, 1.0f));
		vertices.push_back(Vec4(   p,    p, 0.4f, 1.0f));
		vertices.push_back(Vec4(   p,   -p, 0.6f, 1.0f));
		vertices.push_back(Vec4(0.0f,   -p, 0.8f, 1.0f));
		vertices.push_back(Vec4(   p, 0.0f, 0.9f, 1.0f));
		vertices.push_back(Vec4(0.0f,    p, 0.1f, 1.0f));
		vertices.push_back(Vec4(  -p, 0.0f, 0.2f, 1.0f));
	}

	tcu::TestLog&	log	= context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing several large points just outside the clip volume. Expecting an empty image." << tcu::TestLog::EndMessage;

	DrawContext drawContext(context, shaders, vertices, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	drawContext.draw();

	const int	numBlackPixels	= countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
	bool		result			= false;

	if (pointClippingOutside)
	{
		// All pixels must be black -- nothing is drawn.
		result = (numBlackPixels == NUM_RENDER_PIXELS);
	}
	else
	{
		// Rendering pixels without clipping: some pixels should not be black -- something is drawn.
		result = (numBlackPixels < NUM_RENDER_PIXELS);
	}

	return (result ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Wide line clipping
//! Spec: If the primitive is a line segment, then clipping does nothing to it if it lies entirely within the clip volume, and discards it
//!       if it lies entirely outside the volume.
tcu::TestStatus testWideLines (Context& context, const LineOrientation lineOrientation)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_WIDE_LINES);

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	const float delta = 0.1f;  // much smaller than the line width

	std::vector<Vec4> vertices;
	if (lineOrientation == LINE_ORIENTATION_AXIS_ALIGNED)
	{
		// Axis-aligned lines just outside the clip volume.
		const float p = 1.0f + delta;
		const float q = 0.9f;

		vertices.push_back(Vec4(-p, -q, 0.1f, 1.0f));
		vertices.push_back(Vec4(-p,  q, 0.9f, 1.0f));	// line 0
		vertices.push_back(Vec4(-q,  p, 0.1f, 1.0f));
		vertices.push_back(Vec4( q,  p, 0.9f, 1.0f));	// line 1
		vertices.push_back(Vec4( p,  q, 0.1f, 1.0f));
		vertices.push_back(Vec4( p, -q, 0.9f, 1.0f));	// line 2
		vertices.push_back(Vec4( q, -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(-q, -p, 0.9f, 1.0f));	// line 3
	}
	else if (lineOrientation == LINE_ORIENTATION_DIAGONAL)
	{
		// Diagonal lines just outside the clip volume.
		const float p = 2.0f + delta;

		vertices.push_back(Vec4(  -p, 0.0f, 0.1f, 1.0f));
		vertices.push_back(Vec4(0.0f,   -p, 0.9f, 1.0f));	// line 0
		vertices.push_back(Vec4(0.0f,   -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(   p, 0.0f, 0.9f, 1.0f));	// line 1
		vertices.push_back(Vec4(   p, 0.0f, 0.1f, 1.0f));
		vertices.push_back(Vec4(0.0f,    p, 0.9f, 1.0f));	// line 2
		vertices.push_back(Vec4(0.0f,    p, 0.1f, 1.0f));
		vertices.push_back(Vec4(  -p, 0.0f, 0.9f, 1.0f));	// line 3
	}
	else
		DE_ASSERT(0);

	const VkPhysicalDeviceLimits limits = getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;

	const float		lineWidth	= std::min(static_cast<float>(RENDER_SIZE), limits.lineWidthRange[1]);
	tcu::TestLog&	log			= context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing several wide lines just outside the clip volume. Expecting an empty image." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Line width is " << lineWidth << "." << tcu::TestLog::EndMessage;

	DrawContext drawContext(context, shaders, vertices, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, static_cast<deUint32>(RENDER_SIZE), false, false, lineWidth);
	drawContext.draw();

	// All pixels must be black -- nothing is drawn.
	const int numBlackPixels = countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());

	return (numBlackPixels == NUM_RENDER_PIXELS ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

} // ClipVolume ns

namespace ClipDistance
{

struct CaseDefinition
{
	const VkPrimitiveTopology	topology;
	const bool					dynamicIndexing;
	const bool					enableTessellation;
	const bool					enableGeometry;
	const int					numClipDistances;
	const int					numCullDistances;

	CaseDefinition (const VkPrimitiveTopology	topology_,
					const int					numClipDistances_,
					const int					numCullDistances_,
					const bool					enableTessellation_,
					const bool					enableGeometry_,
					const bool					dynamicIndexing_)
		: topology					(topology_)
		, dynamicIndexing			(dynamicIndexing_)
		, enableTessellation		(enableTessellation_)
		, enableGeometry			(enableGeometry_)
		, numClipDistances			(numClipDistances_)
		, numCullDistances			(numCullDistances_)
	{
	}
};

void initPrograms (SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(caseDef.numClipDistances + caseDef.numCullDistances <= MAX_COMBINED_CLIP_AND_CULL_DISTANCES);

	std::string perVertexBlock;
	{
		std::ostringstream str;
		str << "gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n";
		if (caseDef.numClipDistances > 0)
			str << "    float gl_ClipDistance[" << caseDef.numClipDistances << "];\n";
		if (caseDef.numCullDistances > 0)
			str << "    float gl_CullDistance[" << caseDef.numCullDistances << "];\n";
		str << "}";
		perVertexBlock = str.str();
	}

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 v_position;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = v_position;\n"
			<< "    out_color   = vec4(1.0, 0.5 * (v_position.x + 1.0), 0.0, 1.0);\n"
			<< "\n"
			<< "    const int barNdx = gl_VertexIndex / 6;\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_ClipDistance[i] = (barNdx == i ? v_position.y : 0.0);\n";
			if (caseDef.numCullDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n"
					<< "        gl_CullDistance[i] = 0.0;\n";
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_ClipDistance[" << i << "] = (barNdx == " << i << " ? v_position.y : 0.0);\n";
			for (int i = 0; i < caseDef.numCullDistances; ++i)
				src << "    gl_CullDistance[" << i << "] = 0.0;\n";		// don't cull anything
		}
		src	<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	if (caseDef.enableTessellation)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(vertices = " << NUM_PATCH_CONTROL_POINTS << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color[];\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << perVertexBlock << " gl_out[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "    out_color[gl_InvocationID]          = in_color[gl_InvocationID];\n"
			<< "\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_out[gl_InvocationID].gl_ClipDistance[i] = gl_in[gl_InvocationID].gl_ClipDistance[i];\n";
			if (caseDef.numCullDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n"
					<< "        gl_out[gl_InvocationID].gl_CullDistance[i] = gl_in[gl_InvocationID].gl_CullDistance[i];\n";
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_out[gl_InvocationID].gl_ClipDistance[" << i << "] = gl_in[gl_InvocationID].gl_ClipDistance[" << i << "];\n";
			for (int i = 0; i < caseDef.numCullDistances; ++i)
				src << "    gl_out[gl_InvocationID].gl_CullDistance[" << i << "] = gl_in[gl_InvocationID].gl_CullDistance[" << i << "];\n";
		}
		src << "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	if (caseDef.enableTessellation)
	{
		DE_ASSERT(NUM_PATCH_CONTROL_POINTS == 3);  // assumed in shader code

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(triangles, equal_spacing, ccw) in;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    vec3 px     = gl_TessCoord.x * gl_in[0].gl_Position.xyz;\n"
			<< "    vec3 py     = gl_TessCoord.y * gl_in[1].gl_Position.xyz;\n"
			<< "    vec3 pz     = gl_TessCoord.z * gl_in[2].gl_Position.xyz;\n"
			<< "    gl_Position = vec4(px + py + pz, 1.0);\n"
			<< "    out_color   = (in_color[0] + in_color[1] + in_color[2]) / 3.0;\n"
			<< "\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_ClipDistance[i] = gl_TessCoord.x * gl_in[0].gl_ClipDistance[i]\n"
					<< "                           + gl_TessCoord.y * gl_in[1].gl_ClipDistance[i]\n"
					<< "                           + gl_TessCoord.z * gl_in[2].gl_ClipDistance[i];\n";
			if (caseDef.numCullDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n"
					<< "        gl_CullDistance[i] = gl_TessCoord.x * gl_in[0].gl_CullDistance[i]\n"
					<< "                           + gl_TessCoord.y * gl_in[1].gl_CullDistance[i]\n"
					<< "                           + gl_TessCoord.z * gl_in[2].gl_CullDistance[i];\n";
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_ClipDistance[" << i << "] = gl_TessCoord.x * gl_in[0].gl_ClipDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.y * gl_in[1].gl_ClipDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.z * gl_in[2].gl_ClipDistance[" << i << "];\n";
			for (int i = 0; i < caseDef.numCullDistances; ++i)
				src << "    gl_CullDistance[" << i << "] = gl_TessCoord.x * gl_in[0].gl_CullDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.y * gl_in[1].gl_CullDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.z * gl_in[2].gl_CullDistance[" << i << "];\n";
		}
		src << "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	if (caseDef.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = 3) out;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[];\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n";
		for (int vertNdx = 0; vertNdx < 3; ++vertNdx)
		{
			if (vertNdx > 0)
				src << "\n";
			src << "    gl_Position = gl_in[" << vertNdx << "].gl_Position;\n"
				<< "    out_color   = in_color[" << vertNdx << "];\n";
			if (caseDef.dynamicIndexing)
			{
				if (caseDef.numClipDistances > 0)
					src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
						<< "        gl_ClipDistance[i] = gl_in[" << vertNdx << "].gl_ClipDistance[i];\n";
				if (caseDef.numCullDistances > 0)
					src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n"
						<< "        gl_CullDistance[i] = gl_in[" << vertNdx << "].gl_CullDistance[i];\n";
			}
			else
			{
				for (int i = 0; i < caseDef.numClipDistances; ++i)
					src << "    gl_ClipDistance[" << i << "] = gl_in[" << vertNdx << "].gl_ClipDistance[" << i << "];\n";
				for (int i = 0; i < caseDef.numCullDistances; ++i)
					src << "    gl_CullDistance[" << i << "] = gl_in[" << vertNdx << "].gl_CullDistance[" << i << "];\n";
			}
			src << "    EmitVertex();\n";
		}
		src	<< "}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in flat vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(in_color.rgb + vec3(0.0, 0.0, 0.5), 1.0);\n"  // mix with a constant color in case variable wasn't passed correctly through stages
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus testClipDistance (Context& context, const CaseDefinition caseDef)
{
	// Check test requirements
	{
		const InstanceInterface&		vki			= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice	= context.getPhysicalDevice();
		const VkPhysicalDeviceLimits	limits		= getPhysicalDeviceProperties(vki, physDevice).limits;

		FeatureFlags requirements = (FeatureFlags)0;

		if (caseDef.numClipDistances > 0)
			requirements |= FEATURE_SHADER_CLIP_DISTANCE;
		if (caseDef.numCullDistances > 0)
			requirements |= FEATURE_SHADER_CULL_DISTANCE;
		if (caseDef.enableTessellation)
			requirements |= FEATURE_TESSELLATION_SHADER;
		if (caseDef.enableGeometry)
			requirements |= FEATURE_GEOMETRY_SHADER;

		requireFeatures(vki, physDevice, requirements);

		// Check limits for supported features

		if (caseDef.numClipDistances > 0 && limits.maxClipDistances < MAX_CLIP_DISTANCES)
			return tcu::TestStatus::fail("maxClipDistances smaller than the minimum required by the spec");
		if (caseDef.numCullDistances > 0 && limits.maxCullDistances < MAX_CULL_DISTANCES)
			return tcu::TestStatus::fail("maxCullDistances smaller than the minimum required by the spec");
		if (caseDef.numCullDistances > 0 && limits.maxCombinedClipAndCullDistances < MAX_COMBINED_CLIP_AND_CULL_DISTANCES)
			return tcu::TestStatus::fail("maxCombinedClipAndCullDistances smaller than the minimum required by the spec");
	}

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));
	if (caseDef.enableTessellation)
	{
		shaders.push_back(Shader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		context.getBinaryCollection().get("tesc")));
		shaders.push_back(Shader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	context.getBinaryCollection().get("tese")));
	}
	if (caseDef.enableGeometry)
		shaders.push_back(Shader(VK_SHADER_STAGE_GEOMETRY_BIT,	context.getBinaryCollection().get("geom")));

	const int numBars = MAX_COMBINED_CLIP_AND_CULL_DISTANCES;

	std::vector<Vec4> vertices;
	{
		const float	dx = 2.0f / numBars;
		for (int i = 0; i < numBars; ++i)
		{
			const float x = -1.0f + dx * static_cast<float>(i);

			vertices.push_back(Vec4(x,      -1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x,       1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));

			vertices.push_back(Vec4(x,       1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx,  1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));
		}
	}

	tcu::TestLog& log = context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing " << numBars << " colored bars, clipping the first " << caseDef.numClipDistances << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Using " << caseDef.numClipDistances << " ClipDistance(s) and " << caseDef.numCullDistances << " CullDistance(s)" << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Expecting upper half of the clipped bars to be black." << tcu::TestLog::EndMessage;

	DrawContext drawContext(context, shaders, vertices, caseDef.topology);
	drawContext.draw();

	// Count black pixels in the whole image.
	const int numBlackPixels		= countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
	const IVec2	clipRegion			= IVec2(caseDef.numClipDistances * RENDER_SIZE / numBars, RENDER_SIZE / 2);
	const int expectedClippedPixels	= clipRegion.x() * clipRegion.y();
	// Make sure the bottom half has no black pixels (possible if image became corrupted).
	const int guardPixels			= countPixels(drawContext.getColorPixels(), IVec2(0, RENDER_SIZE/2), clipRegion, Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());

	return (numBlackPixels == expectedClippedPixels && guardPixels == 0 ? tcu::TestStatus::pass("OK")
																		: tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

} // ClipDistance ns

namespace ClipDistanceComplementarity
{

void initPrograms (SourceCollections& programCollection, const int numClipDistances)
{
	// Vertex shader
	{
		DE_ASSERT(numClipDistances > 0);
		const int clipDistanceLastNdx = numClipDistances - 1;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 v_position;    // we are passing ClipDistance in w component\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n"
			<< "    float gl_ClipDistance[" << numClipDistances << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position        = vec4(v_position.xyz, 1.0);\n";
		for (int i = 0; i < clipDistanceLastNdx; ++i)
			src << "    gl_ClipDistance[" << i << "] = 0.0;\n";
		src << "    gl_ClipDistance[" << clipDistanceLastNdx << "] = v_position.w;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, 1.0, 1.0, 0.5);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus testComplementarity (Context& context, const int numClipDistances)
{
	// Check test requirements
	{
		const InstanceInterface&		vki			= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice	= context.getPhysicalDevice();

		requireFeatures(vki, physDevice, FEATURE_SHADER_CLIP_DISTANCE);
	}

	std::vector<Shader> shaders;
	shaders.push_back(Shader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")));
	shaders.push_back(Shader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	std::vector<Vec4> vertices;
	{
		de::Random	rnd						(1234);
		const int	numSections				= 16;
		const int	numVerticesPerSection	= 4;	// logical verticies, due to triangle list topology we actually use 6 per section

		DE_ASSERT(RENDER_SIZE_LARGE % numSections == 0);

		std::vector<float> clipDistances(numVerticesPerSection * numSections);
		for (int i = 0; i < static_cast<int>(clipDistances.size()); ++i)
			clipDistances[i] = rnd.getFloat(-1.0f, 1.0f);

		// Two sets of identical primitives, but with a different ClipDistance sign.
		for (int setNdx = 0; setNdx < 2; ++setNdx)
		{
			const float sign = (setNdx == 0 ? 1.0f : -1.0f);
			const float	dx	 = 2.0f / static_cast<float>(numSections);

			for (int i = 0; i < numSections; ++i)
			{
				const int	ndxBase	= numVerticesPerSection * i;
				const float x		= -1.0f + dx * static_cast<float>(i);
				const Vec4	p0		= Vec4(x,      -1.0f, 0.0f, sign * clipDistances[ndxBase + 0]);
				const Vec4	p1		= Vec4(x,       1.0f, 0.0f, sign * clipDistances[ndxBase + 1]);
				const Vec4	p2		= Vec4(x + dx,  1.0f, 0.0f, sign * clipDistances[ndxBase + 2]);
				const Vec4	p3		= Vec4(x + dx, -1.0f, 0.0f, sign * clipDistances[ndxBase + 3]);

				vertices.push_back(p0);
				vertices.push_back(p1);
				vertices.push_back(p2);

				vertices.push_back(p2);
				vertices.push_back(p3);
				vertices.push_back(p0);
			}
		}
	}

	tcu::TestLog& log = context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Draw two sets of primitives with blending, differing only with ClipDistance sign." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Using " << numClipDistances << " clipping plane(s), one of them possibly having negative values." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Expecting a uniform gray area, no missing (black) nor overlapped (white) pixels." << tcu::TestLog::EndMessage;

	DrawContext drawContext(context, shaders, vertices, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, static_cast<deUint32>(RENDER_SIZE_LARGE), false, true);
	drawContext.draw();

	const int numGrayPixels		= countPixels(drawContext.getColorPixels(), Vec4(0.5f, 0.5f, 0.5f, 1.0f), Vec4(0.02f, 0.02f, 0.02f, 0.0f));
	const int numExpectedPixels	= RENDER_SIZE_LARGE * RENDER_SIZE_LARGE;

	return (numGrayPixels == numExpectedPixels ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

} // ClipDistanceComplementarity ns

void addClippingTests (tcu::TestCaseGroup* clippingTestsGroup)
{
	tcu::TestContext& testCtx = clippingTestsGroup->getTestContext();

	// Clipping against the clip volume
	{
		using namespace ClipVolume;

		static const VkPrimitiveTopology cases[] =
		{
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		};

		MovePtr<tcu::TestCaseGroup> clipVolumeGroup(new tcu::TestCaseGroup(testCtx, "clip_volume", "clipping with the clip volume"));

		// Fully inside the clip volume
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "inside", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", initPrograms, testPrimitivesInside, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Fully outside the clip volume
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "outside", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", initPrograms, testPrimitivesOutside, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Depth clamping
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "depth_clamp", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", initPrograms, testPrimitivesDepthClamp, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Large points and wide lines
		{
			// \note For both points and lines, if an unsupported size/width is selected, the nearest supported size will be chosen.
			//       We do have to check for feature support though.

			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "clipped", ""));

			addFunctionCaseWithPrograms(group.get(), "large_points", "", initProgramsPointSize, testLargePoints);

			addFunctionCaseWithPrograms<LineOrientation>(group.get(), "wide_lines_axis_aligned", "", initPrograms, testWideLines, LINE_ORIENTATION_AXIS_ALIGNED);
			addFunctionCaseWithPrograms<LineOrientation>(group.get(), "wide_lines_diagonal",	 "", initPrograms, testWideLines, LINE_ORIENTATION_DIAGONAL);

			clipVolumeGroup->addChild(group.release());
		}

		clippingTestsGroup->addChild(clipVolumeGroup.release());
	}

	// User-defined clip planes
	{
		MovePtr<tcu::TestCaseGroup> clipDistanceGroup(new tcu::TestCaseGroup(testCtx, "user_defined", "user-defined clip planes"));

		// ClipDistance, CullDistance and maxCombinedClipAndCullDistances usage
		{
			using namespace ClipDistance;

			static const struct
			{
				const char* const	groupName;
				const char* const	description;
				bool				useCullDistance;
			} caseGroups[] =
			{
				{ "clip_distance",		"use ClipDistance",										false },
				{ "clip_cull_distance",	"use ClipDistance and CullDistance at the same time",	true  },
			};

			const deUint32 flagTessellation = 1u << 0;
			const deUint32 flagGeometry		= 1u << 1;

			for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(caseGroups); ++groupNdx)
			for (int indexingMode = 0; indexingMode < 2; ++indexingMode)
			{
				const bool			dynamicIndexing	= (indexingMode == 1);
				const std::string	mainGroupName	= de::toString(caseGroups[groupNdx].groupName) + (dynamicIndexing ? "_dynamic_index" : "");

				MovePtr<tcu::TestCaseGroup>	mainGroup(new tcu::TestCaseGroup(testCtx, mainGroupName.c_str(), ""));

				for (deUint32 shaderMask = 0u; shaderMask <= (flagTessellation | flagGeometry); ++shaderMask)
				{
					const bool			useTessellation	= (shaderMask & flagTessellation) != 0;
					const bool			useGeometry		= (shaderMask & flagGeometry) != 0;
					const std::string	shaderGroupName	= std::string("vert") + (useTessellation ? "_tess" : "") + (useGeometry ? "_geom" : "");

					MovePtr<tcu::TestCaseGroup>	shaderGroup(new tcu::TestCaseGroup(testCtx, shaderGroupName.c_str(), ""));

					for (int numClipPlanes = 1; numClipPlanes <= MAX_CLIP_DISTANCES; ++numClipPlanes)
					{
						const int					numCullPlanes	= (caseGroups[groupNdx].useCullDistance
																		? std::min(static_cast<int>(MAX_CULL_DISTANCES), MAX_COMBINED_CLIP_AND_CULL_DISTANCES - numClipPlanes)
																		: 0);
						const std::string			caseName		= de::toString(numClipPlanes) + (numCullPlanes > 0 ? "_" + de::toString(numCullPlanes) : "");
						const VkPrimitiveTopology	topology		= (useTessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

						addFunctionCaseWithPrograms<CaseDefinition>(
							shaderGroup.get(), caseName, caseGroups[groupNdx].description, initPrograms, testClipDistance,
							CaseDefinition(topology, numClipPlanes, numCullPlanes, useTessellation, useGeometry, dynamicIndexing));
					}
					mainGroup->addChild(shaderGroup.release());
				}
				clipDistanceGroup->addChild(mainGroup.release());
			}
		}

		// Complementarity criterion (i.e. clipped and not clipped areas must add up to a complete primitive with no holes nor overlap)
		{
			using namespace ClipDistanceComplementarity;

			MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "complementarity", ""));

			for (int numClipDistances = 1; numClipDistances <= MAX_CLIP_DISTANCES; ++numClipDistances)
				addFunctionCaseWithPrograms<int>(group.get(), de::toString(numClipDistances).c_str(), "", initPrograms, testComplementarity, numClipDistances);

			clippingTestsGroup->addChild(group.release());
		}

		clippingTestsGroup->addChild(clipDistanceGroup.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "clipping", "Clipping tests", addClippingTests);
}

} // clipping
} // vkt
