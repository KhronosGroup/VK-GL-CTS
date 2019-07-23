/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Google Inc.
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
 * \brief Utility for generating simple work
 *//*--------------------------------------------------------------------*/

#include "vktDrawUtil.hpp"
#include "rrMultisamplePixelBufferAccess.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "rrRenderer.hpp"
#include "rrRenderState.hpp"
#include "rrPrimitiveTypes.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "deArrayUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace drawutil
{

using namespace de;
using namespace tcu;
using namespace vk;

static VkCompareOp mapCompareOp (rr::TestFunc compareFunc)
{
	switch (compareFunc)
	{
		case rr::TESTFUNC_NEVER:				return VK_COMPARE_OP_NEVER;
		case rr::TESTFUNC_LESS:					return VK_COMPARE_OP_LESS;
		case rr::TESTFUNC_EQUAL:				return VK_COMPARE_OP_EQUAL;
		case rr::TESTFUNC_LEQUAL:				return VK_COMPARE_OP_LESS_OR_EQUAL;
		case rr::TESTFUNC_GREATER:				return VK_COMPARE_OP_GREATER;
		case rr::TESTFUNC_NOTEQUAL:				return VK_COMPARE_OP_NOT_EQUAL;
		case rr::TESTFUNC_GEQUAL:				return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case rr::TESTFUNC_ALWAYS:				return VK_COMPARE_OP_ALWAYS;
		default:
			DE_ASSERT(false);
	}
	return VK_COMPARE_OP_LAST;
}

rr::PrimitiveType mapVkPrimitiveToRRPrimitive(const vk::VkPrimitiveTopology& primitiveTopology)
{
	static const rr::PrimitiveType primitiveTypeTable[] =
	{
		rr::PRIMITIVETYPE_POINTS,
		rr::PRIMITIVETYPE_LINES,
		rr::PRIMITIVETYPE_LINE_STRIP,
		rr::PRIMITIVETYPE_TRIANGLES,
		rr::PRIMITIVETYPE_TRIANGLE_STRIP,
		rr::PRIMITIVETYPE_TRIANGLE_FAN,
		rr::PRIMITIVETYPE_LINES_ADJACENCY,
		rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY,
		rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY,
		rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY
	};

	return de::getSizedArrayElement<vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST>(primitiveTypeTable, primitiveTopology);
}

Move<VkCommandBuffer> makeCommandBuffer (const DeviceInterface& vk, const VkDevice device, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo info =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType		sType;
		DE_NULL,											// const void*			pNext;
		commandPool,										// VkCommandPool		commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel	level;
		1u,													// deUint32				commandBufferCount;
	};
	return allocateCommandBuffer(vk, device, &info);
}

std::string getPrimitiveTopologyShortName (const VkPrimitiveTopology topology)
{
	std::string name(getPrimitiveTopologyName(topology));
	return de::toLower(name.substr(22));
}

DrawState::DrawState(const vk::VkPrimitiveTopology topology_, deUint32 renderWidth_, deUint32 renderHeight_, const int subpixelBits_)
	: topology					(topology_)
	, colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, renderSize				(tcu::UVec2(renderWidth_, renderHeight_))
	, depthClampEnable			(false)
	, depthTestEnable			(false)
	, depthWriteEnable			(false)
	, compareOp					(rr::TESTFUNC_LESS)
	, depthBoundsTestEnable		(false)
	, blendEnable				(false)
	, lineWidth					(1.0)
	, numPatchControlPoints		(0)
	, numSamples				(VK_SAMPLE_COUNT_1_BIT)
	, sampleShadingEnable		(false)
	, subpixelBits			(subpixelBits_)
	, explicitDepthClipEnable	(false)
	, depthClipEnable			(false)
{
	DE_ASSERT(renderSize.x() != 0 && renderSize.y() != 0);
}

ReferenceDrawContext::~ReferenceDrawContext (void)
{
}

void ReferenceDrawContext::draw (void)
{
	m_refImage.setStorage(vk::mapVkFormat(m_drawState.colorFormat), m_drawState.renderSize.x(), m_drawState.renderSize.y());
	tcu::clear(m_refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	{
		const rr::Program						program(&m_vertexShader, &m_fragmentShader);
		const rr::MultisamplePixelBufferAccess	referenceColorBuffer = rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(m_refImage.getAccess());
		const rr::RenderTarget					renderTarget(referenceColorBuffer);
		const rr::RenderState					renderState((rr::ViewportState(referenceColorBuffer)), m_drawState.subpixelBits, rr::VIEWPORTORIENTATION_UPPER_LEFT);
		const rr::Renderer						renderer;
		const rr::VertexAttrib					vertexAttrib[] =
		{
			rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &m_drawCallData.vertices[0])
		};

		renderer.draw(rr::DrawCommand(	renderState,
										renderTarget,
										program,
										DE_LENGTH_OF_ARRAY(vertexAttrib),
										&vertexAttrib[0],
										rr::PrimitiveList(mapVkPrimitiveToRRPrimitive(m_drawState.topology), (int)m_drawCallData.vertices.size(), 0)));

	}

}

tcu::ConstPixelBufferAccess ReferenceDrawContext::getColorPixels (void) const
{
	return tcu::ConstPixelBufferAccess( m_refImage.getAccess().getFormat(),
										m_refImage.getAccess().getWidth(),
										m_refImage.getAccess().getHeight(),
										m_refImage.getAccess().getDepth(),
										m_refImage.getAccess().getDataPtr());
}

VulkanDrawContext::VulkanDrawContext (Context&				context,
									  const DrawState&		drawState,
									  const DrawCallData&	drawCallData,
									  const VulkanProgram&	vulkanProgram)
	: DrawContext	(drawState, drawCallData)
	, m_context		(context)
	, m_program		(vulkanProgram)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	VkImageSubresourceRange	colorSubresourceRange;
	Move<VkSampler>			sampler;

	// Command buffer
	{
		m_cmdPool			= makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
		m_cmdBuffer			= makeCommandBuffer(vk, device, *m_cmdPool);
	}

	// Color attachment image
	{
		const VkImageUsageFlags usage			= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		colorSubresourceRange					= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			(VkImageCreateFlags)0,														// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_drawState.colorFormat,													// VkFormat					format;
			makeExtent3D(m_drawState.renderSize.x(), m_drawState.renderSize.y(), 1u),	// VkExtent3D				extent;
			1u,																			// uint32_t					mipLevels;
			1u,																			// uint32_t					arrayLayers;
			(VkSampleCountFlagBits)m_drawState.numSamples,								// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			usage,																		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			0u,																			// uint32_t					queueFamilyIndexCount;
			DE_NULL,																	// const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		m_colorImage = MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
		m_colorImageView = makeImageView(vk, device, **m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_drawState.colorFormat, colorSubresourceRange);

		// Buffer to copy attachment data after rendering

		const VkDeviceSize bitmapSize = tcu::getPixelSize(mapVkFormat(m_drawState.colorFormat)) * m_drawState.renderSize.x() * m_drawState.renderSize.y();
		m_colorAttachmentBuffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(bitmapSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

		{
			const Allocation& alloc = m_colorAttachmentBuffer->getAllocation();
			deMemset(alloc.getHostPtr(), 0, (size_t)bitmapSize);
			flushAlloc(vk, device, alloc);
		}
	}

	// Vertex buffer
	{
		const VkDeviceSize bufferSize = m_drawCallData.vertices.size() * sizeof(m_drawCallData.vertices[0]);
		m_vertexBuffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_vertexBuffer->getAllocation();
		deMemcpy(alloc.getHostPtr(), &m_drawCallData.vertices[0], (size_t)bufferSize);
		flushAlloc(vk, device, alloc);
	}

	// bind descriptor sets
	{
		m_pipelineLayout = makePipelineLayout(vk, device, vulkanProgram.descriptorSetLayout);
	}

	// Renderpass
	{
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		const VkAttachmentDescription attachDescriptors[] =
		{
			{
				(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
				m_drawState.colorFormat,							// VkFormat							format;
				(VkSampleCountFlagBits)m_drawState.numSamples,		// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					finalLayout;
			},
			{
				(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags
				m_drawState.depthFormat,							// VkFormat							format
				(VkSampleCountFlagBits)m_drawState.numSamples,		// VkSampleCountFlagBits			samples
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout					finalLayout

			}
		};

		const VkAttachmentReference attachmentReferences[] =
		{
			{
				0u,													// uint32_t			attachment
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout
			},
			{
				1u,													// uint32_t			attachment
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
			},
			{
				VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
				VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
			}
		};

		attachmentDescriptions.push_back(attachDescriptors[0]);
		if (!!vulkanProgram.depthImageView)
			attachmentDescriptions.push_back(attachDescriptors[1]);

		deUint32 depthReferenceNdx = !!vulkanProgram.depthImageView ? 1 : 2;
		const VkSubpassDescription subpassDescription =
		{
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
			0u,													// deUint32							inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
			1u,													// deUint32							colorAttachmentCount;
			&attachmentReferences[0],							// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
			&attachmentReferences[depthReferenceNdx],			// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,													// deUint32							preserveAttachmentCount;
			DE_NULL												// const deUint32*					pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
			(deUint32)attachmentDescriptions.size(),			// deUint32							attachmentCount;
			&attachmentDescriptions[0],							// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, device, &renderPassInfo);
	}

	// Framebuffer
	{
		std::vector<VkImageView>	attachmentBindInfos;
		deUint32					numAttachments;
		attachmentBindInfos.push_back(*m_colorImageView);
		if (!!vulkanProgram.depthImageView)
			attachmentBindInfos.push_back(vulkanProgram.depthImageView);

		numAttachments = (deUint32)(attachmentBindInfos.size());
		const VkFramebufferCreateInfo framebufferInfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,										// const void*							pNext;
			(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags				flags;
			*m_renderPass,									// VkRenderPass							renderPass;
			numAttachments,									// uint32_t								attachmentCount;
			&attachmentBindInfos[0],						// const VkImageView*					pAttachments;
			m_drawState.renderSize.x(),						// uint32_t								width;
			m_drawState.renderSize.y(),						// uint32_t								height;
			1u,												// uint32_t								layers;
		};

		m_framebuffer = createFramebuffer(vk, device, &framebufferInfo);
	}

	// Graphics pipeline
	{
		VkShaderModule	vertShader			= DE_NULL;
		VkShaderModule	tessControlShader	= DE_NULL;
		VkShaderModule	tessEvalShader		= DE_NULL;
		VkShaderModule	geomShader			= DE_NULL;
		VkShaderModule	fragShader			= DE_NULL;

		DE_ASSERT(m_drawState.topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST || m_drawState.numPatchControlPoints > 0);

		const std::vector<VkViewport>	viewports	(1, makeViewport(m_drawState.renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_drawState.renderSize));

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
			m_drawState.depthClampEnable,									// VkBool32									depthClampEnable;
			VK_FALSE,														// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			m_drawState.lineWidth,											// float									lineWidth;
		};

		VkPipelineRasterizationDepthClipStateCreateInfoEXT pipelineRasterizationDepthCliptateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,	// VkStructureType										sType;
			DE_NULL,																	// const void*											pNext;
			(VkPipelineRasterizationDepthClipStateCreateFlagsEXT)0,						// VkPipelineRasterizationDepthClipStateCreateFlagsEXT	flags;
			m_drawState.depthClipEnable,												// VkBool32												depthClipEnable;
		};
		if (m_drawState.explicitDepthClipEnable)
			pipelineRasterizationStateInfo.pNext = &pipelineRasterizationDepthCliptateInfo;

		const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
			(VkSampleCountFlagBits)m_drawState.numSamples,				// VkSampleCountFlagBits					rasterizationSamples;
			m_drawState.sampleShadingEnable ? VK_TRUE : VK_FALSE,		// VkBool32									sampleShadingEnable;
			m_drawState.sampleShadingEnable ? 1.0f : 0.0f,				// float									minSampleShading;
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

		if (m_drawState.depthBoundsTestEnable && !context.getDeviceFeatures().depthBounds)
			TCU_THROW(NotSupportedError, "depthBounds not supported");

		const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
			m_drawState.depthTestEnable,								// VkBool32									depthTestEnable;
			m_drawState.depthWriteEnable,								// VkBool32									depthWriteEnable;
			mapCompareOp(m_drawState.compareOp),						// VkCompareOp								depthCompareOp;
			m_drawState.depthBoundsTestEnable,							// VkBool32									depthBoundsTestEnable
			VK_FALSE,													// VkBool32									stencilTestEnable;
			stencilOpState,												// VkStencilOpState							front;
			stencilOpState,												// VkStencilOpState							back;
			0.0f,														// float									minDepthBounds;
			1.0f,														// float									maxDepthBounds;
		};

		const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
		{
			m_drawState.blendEnable,			// VkBool32					blendEnable;
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

		VkShaderStageFlags stageFlags = (VkShaderStageFlags)0;

		DE_ASSERT(m_program.shaders.size() <= MAX_NUM_SHADER_MODULES);
		for (deUint32 shaderNdx = 0; shaderNdx < m_program.shaders.size(); ++shaderNdx)
		{
			m_shaderModules[shaderNdx] = createShaderModule(vk, device, *m_program.shaders[shaderNdx].binary, (VkShaderModuleCreateFlags)0);

			stageFlags |= m_program.shaders[shaderNdx].stage;

			switch(m_program.shaders[shaderNdx].stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:
					vertShader = *m_shaderModules[shaderNdx];
					break;
				case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
					tessControlShader = *m_shaderModules[shaderNdx];
					break;
				case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
					tessEvalShader = *m_shaderModules[shaderNdx];
					break;
				case VK_SHADER_STAGE_GEOMETRY_BIT:
					geomShader = *m_shaderModules[shaderNdx];
					break;
				default:
					DE_ASSERT(m_program.shaders[shaderNdx].stage == VK_SHADER_STAGE_FRAGMENT_BIT);
					fragShader = *m_shaderModules[shaderNdx];
					break;
			}
		}

		DE_ASSERT(
			(m_drawState.topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) ||
			(stageFlags & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)));

		m_pipeline = makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
										  device,								// const VkDevice                                device
										  *m_pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
										  vertShader,							// const VkShaderModule                          vertexShaderModule
										  tessControlShader,					// const VkShaderModule                          tessellationControlShaderModule
										  tessEvalShader,						// const VkShaderModule                          tessellationEvalShaderModule
										  geomShader,							// const VkShaderModule                          geometryShaderModule
										  fragShader,							// const VkShaderModule                          fragmentShaderModule
										  *m_renderPass,						// const VkRenderPass                            renderPass
										  viewports,							// const std::vector<VkViewport>&                viewports
										  scissors,								// const std::vector<VkRect2D>&                  scissors
										  m_drawState.topology,					// const VkPrimitiveTopology                     topology
										  0u,									// const deUint32                                subpass
										  m_drawState.numPatchControlPoints,	// const deUint32                                patchControlPoints
										  DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
										  &pipelineRasterizationStateInfo,		// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
										  &pipelineMultisampleStateInfo,		// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
										  &pipelineDepthStencilStateInfo,		// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
										  &pipelineColorBlendStateInfo);		// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
	}

	// Record commands
	{
		const VkDeviceSize zeroOffset = 0ull;

		beginCommandBuffer(vk, *m_cmdBuffer);
		if (!!vulkanProgram.descriptorSet)
			vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &vulkanProgram.descriptorSet, 0u, DE_NULL);

		// Begin render pass
		if (!!vulkanProgram.depthImageView)
			beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_drawState.renderSize.x(), m_drawState.renderSize.y()), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0);
		else
			beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_drawState.renderSize.x(), m_drawState.renderSize.y()), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(**m_vertexBuffer), &zeroOffset);

		vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_drawCallData.vertices.size()), 1u, 0u, 0u);
		endRenderPass(vk, *m_cmdBuffer);

		// Barrier: draw -> copy from image
		{
			const VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				**m_colorImage, colorSubresourceRange);

			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
				0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
		}

		// Resolve multisample image
		{
			if (m_drawState.numSamples != VK_SAMPLE_COUNT_1_BIT)
			{
				const VkImageResolve imageResolve =
				{
					makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),
					{ 0, 0, 0},
					makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),
					{ 0, 0, 0},
					makeExtent3D(m_drawState.renderSize.x(), m_drawState.renderSize.y(), 1u)
				};

				const VkImageCreateInfo resolveImageCreateInfo =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,				// VkStructureType			sType
					DE_NULL,											// const void*				pNext
					(VkImageCreateFlags)0,								// VkImageCreateFlags		flags
					VK_IMAGE_TYPE_2D,									// VkImageType				imageType
					m_drawState.colorFormat,							// VkFormat					format
					makeExtent3D(m_drawState.renderSize.x(),			// VkExtent3D				extent;
							m_drawState.renderSize.y(), 1u),
					1u,													// uint32_t					mipLevels
					1u,													// uint32_t					arrayLayers
					VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits	samples
					VK_IMAGE_TILING_OPTIMAL,							// VkImaageTiling			tiling
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |					// VkImageUsageFlags		usage
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					VK_SHARING_MODE_EXCLUSIVE,							// VkSharingModeExclusive	sharingMode
					0u,													// uint32_t					queueFamilyIndexCount
					DE_NULL,											// const uint32_t*			pQueueFamilyIndices
					VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout			initialLayout
				};

				m_resolveImage = MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, resolveImageCreateInfo, MemoryRequirement::Any));

				const VkImageMemoryBarrier resolveBarrier = makeImageMemoryBarrier(
						0u, VK_ACCESS_TRANSFER_READ_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						**m_resolveImage, colorSubresourceRange);

				vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
						0u, DE_NULL, 0u, DE_NULL, 1u, &resolveBarrier);

				vk.cmdResolveImage(*m_cmdBuffer, **m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						**m_resolveImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &imageResolve);

				const VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					**m_resolveImage, colorSubresourceRange);

				vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
					0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
			}
			else
				m_resolveImage = m_colorImage;

			const VkBufferImageCopy copyRegion = makeBufferImageCopy(
				makeExtent3D(m_drawState.renderSize.x(), m_drawState.renderSize.y(), 1u),
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			vk.cmdCopyImageToBuffer(*m_cmdBuffer, **m_resolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_colorAttachmentBuffer, 1u, &copyRegion);
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

VulkanDrawContext::~VulkanDrawContext (void)
{
}

void VulkanDrawContext::draw (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	const VkQueue			queue		= m_context.getUniversalQueue();
	tcu::TestLog&			log			= m_context.getTestContext().getLog();

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	log << tcu::LogImageSet("attachments", "") << tcu::LogImage("color0", "", getColorPixels()) << tcu::TestLog::EndImageSet;
}

tcu::ConstPixelBufferAccess VulkanDrawContext::getColorPixels (void) const
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	const Allocation& alloc = m_colorAttachmentBuffer->getAllocation();
	invalidateAlloc(vk, device, alloc);

	return tcu::ConstPixelBufferAccess(mapVkFormat(m_drawState.colorFormat), m_drawState.renderSize.x(), m_drawState.renderSize.y(), 1u, alloc.getHostPtr());
}
} // drawutil
} // vkt
