/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Tessellation Utilities
 *//*--------------------------------------------------------------------*/

#include "vktTessellationUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "deMath.h"

namespace vkt
{
namespace tessellation
{

using namespace vk;

VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage, const deUint32 numArrayLayers)
{
	const VkImageCreateInfo imageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,							// VkImageType              imageType;
		format,										// VkFormat                 format;
		makeExtent3D(size.x(), size.y(), 1),		// VkExtent3D               extent;
		1u,											// uint32_t                 mipLevels;
		numArrayLayers,								// uint32_t                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits    samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling            tiling;
		usage,										// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
		0u,											// uint32_t                 queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
	};
	return imageInfo;
}

void beginRenderPassWithRasterizationDisabled (const DeviceInterface&	vk,
											   const VkCommandBuffer	commandBuffer,
											   const VkRenderPass		renderPass,
											   const VkFramebuffer		framebuffer)
{
	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, makeRect2D(0, 0, 0u, 0u));
}

Move<VkRenderPass> makeRenderPassWithoutAttachments (const DeviceInterface&	vk,
													 const VkDevice			device)
{
	const VkAttachmentReference unusedAttachment =
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
		0u,													// deUint32							colorAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		&unusedAttachment,									// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
		0u,													// deUint32							attachmentCount;
		DE_NULL,											// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setShader (const DeviceInterface&			vk,
															 const VkDevice					device,
															 const VkShaderStageFlagBits	stage,
															 const ProgramBinary&			binary,
															 const VkSpecializationInfo*	specInfo)
{
	VkShaderModule module;
	switch (stage)
	{
		case (VK_SHADER_STAGE_VERTEX_BIT):
			DE_ASSERT(m_vertexShaderModule.get() == DE_NULL);
			m_vertexShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_vertexShaderModule;
			break;

		case (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT):
			DE_ASSERT(m_tessControlShaderModule.get() == DE_NULL);
			m_tessControlShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_tessControlShaderModule;
			break;

		case (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT):
			DE_ASSERT(m_tessEvaluationShaderModule.get() == DE_NULL);
			m_tessEvaluationShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_tessEvaluationShaderModule;
			break;

		case (VK_SHADER_STAGE_GEOMETRY_BIT):
			DE_ASSERT(m_geometryShaderModule.get() == DE_NULL);
			m_geometryShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_geometryShaderModule;
			break;

		case (VK_SHADER_STAGE_FRAGMENT_BIT):
			DE_ASSERT(m_fragmentShaderModule.get() == DE_NULL);
			m_fragmentShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_fragmentShaderModule;
			break;

		default:
			DE_FATAL("Invalid shader stage");
			return *this;
	}

	const VkPipelineShaderStageCreateInfo pipelineShaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
		stage,													// VkShaderStageFlagBits				stage;
		module,													// VkShaderModule						module;
		"main",													// const char*							pName;
		specInfo,												// const VkSpecializationInfo*			pSpecializationInfo;
	};

	m_shaderStageFlags |= stage;
	m_shaderStages.push_back(pipelineShaderStageInfo);

	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setVertexInputSingleAttribute (const VkFormat vertexFormat, const deUint32 stride)
{
	const VkVertexInputBindingDescription bindingDesc =
	{
		0u,									// uint32_t				binding;
		stride,								// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,		// VkVertexInputRate	inputRate;
	};
	const VkVertexInputAttributeDescription attributeDesc =
	{
		0u,									// uint32_t			location;
		0u,									// uint32_t			binding;
		vertexFormat,						// VkFormat			format;
		0u,									// uint32_t			offset;
	};

	m_vertexInputBindings.clear();
	m_vertexInputBindings.push_back(bindingDesc);

	m_vertexInputAttributes.clear();
	m_vertexInputAttributes.push_back(attributeDesc);

	return *this;
}

template<typename T>
inline const T* dataPointer (const std::vector<T>& vec)
{
	return (vec.size() != 0 ? &vec[0] : DE_NULL);
}

Move<VkPipeline> GraphicsPipelineBuilder::build (const DeviceInterface&	vk,
												 const VkDevice			device,
												 const VkPipelineLayout	pipelineLayout,
												 const VkRenderPass		renderPass)
{
	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
		static_cast<deUint32>(m_vertexInputBindings.size()),			// uint32_t                                    vertexBindingDescriptionCount;
		dataPointer(m_vertexInputBindings),								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		static_cast<deUint32>(m_vertexInputAttributes.size()),			// uint32_t                                    vertexAttributeDescriptionCount;
		dataPointer(m_vertexInputAttributes),							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPrimitiveTopology topology = (m_shaderStageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
																										 : m_primitiveTopology;
	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags     flags;
		topology,														// VkPrimitiveTopology                         topology;
		VK_FALSE,														// VkBool32                                    primitiveRestartEnable;
	};

	const VkPipelineTessellationDomainOriginStateCreateInfo tessellationDomainOriginStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,
		DE_NULL,
		(!m_tessellationDomainOrigin ? VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT : *m_tessellationDomainOrigin)
	};
	const VkPipelineTessellationStateCreateInfo pipelineTessellationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType                             sType;
		(!m_tessellationDomainOrigin ? DE_NULL : &tessellationDomainOriginStateInfo),
		(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags      flags;
		m_patchControlPoints,											// uint32_t                                    patchControlPoints;
	};

	const VkViewport	viewport	= makeViewport(m_renderSize);
	const VkRect2D		scissor		= makeRect2D(m_renderSize);

	const bool haveRenderSize = m_renderSize.x() > 0 && m_renderSize.y() > 0;

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,												// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags;
		1u,														// uint32_t                                    viewportCount;
		haveRenderSize ? &viewport : DE_NULL,					// const VkViewport*                           pViewports;
		1u,														// uint32_t                                    scissorCount;
		haveRenderSize ? &scissor : DE_NULL,					// const VkRect2D*                             pScissors;
	};

	const bool isRasterizationDisabled = ((m_shaderStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) == 0);
	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		isRasterizationDisabled,										// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		m_cullModeFlags,												// VkCullModeFlags							cullMode;
		m_frontFace,													// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
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

	std::vector<VkDynamicState> dynamicStates;
	if (!haveRenderSize && !isRasterizationDisabled)
	{
		dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
	}

	const VkPipelineDynamicStateCreateInfo pipelineDynamicStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0,														// VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),			// uint32_t								dynamicStateCount;
		(dynamicStates.empty() ? DE_NULL : &dynamicStates[0]),	// const VkDynamicState*				pDynamicStates;
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,						// VkStructureType									sType;
		DE_NULL,																// const void*										pNext;
		(VkPipelineCreateFlags)0,												// VkPipelineCreateFlags							flags;
		static_cast<deUint32>(m_shaderStages.size()),							// deUint32											stageCount;
		&m_shaderStages[0],														// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,													// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,										// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		(m_shaderStageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? &pipelineTessellationStateInfo : DE_NULL), // const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		(isRasterizationDisabled ? DE_NULL : &pipelineViewportStateInfo),		// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,										// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		(isRasterizationDisabled ? DE_NULL : &pipelineMultisampleStateInfo),	// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		(isRasterizationDisabled ? DE_NULL : &pipelineDepthStencilStateInfo),	// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		(isRasterizationDisabled ? DE_NULL : &pipelineColorBlendStateInfo),		// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(dynamicStates.empty() ? DE_NULL : &pipelineDynamicStateInfo),			// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,															// VkPipelineLayout									layout;
		renderPass,																// VkRenderPass										renderPass;
		0u,																		// deUint32											subpass;
		DE_NULL,																// VkPipeline										basePipelineHandle;
		0,																		// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

float getClampedTessLevel (const SpacingMode mode, const float tessLevel)
{
	switch (mode)
	{
		case SPACINGMODE_EQUAL:				return de::max(1.0f, tessLevel);
		case SPACINGMODE_FRACTIONAL_ODD:	return de::max(1.0f, tessLevel);
		case SPACINGMODE_FRACTIONAL_EVEN:	return de::max(2.0f, tessLevel);
		default:
			DE_ASSERT(false);
			return 0.0f;
	}
}

int getRoundedTessLevel (const SpacingMode mode, const float clampedTessLevel)
{
	static const int minimumMaxTessGenLevel = 64;	//!< Minimum maxTessellationGenerationLevel defined by the spec.

	int result = (int)deFloatCeil(clampedTessLevel);

	switch (mode)
	{
		case SPACINGMODE_EQUAL:											break;
		case SPACINGMODE_FRACTIONAL_ODD:	result += 1 - result % 2;	break;
		case SPACINGMODE_FRACTIONAL_EVEN:	result += result % 2;		break;
		default:
			DE_ASSERT(false);
	}
	DE_ASSERT(de::inRange<int>(result, 1, minimumMaxTessGenLevel));
	DE_UNREF(minimumMaxTessGenLevel);

	return result;
}

int getClampedRoundedTessLevel (const SpacingMode mode, const float tessLevel)
{
	return getRoundedTessLevel(mode, getClampedTessLevel(mode, tessLevel));
}

void getClampedRoundedTriangleTessLevels (const SpacingMode	spacingMode,
										  const float*		innerSrc,
										  const float*		outerSrc,
										  int*				innerDst,
										  int*				outerDst)
{
	innerDst[0] = getClampedRoundedTessLevel(spacingMode, innerSrc[0]);
	for (int i = 0; i < 3; i++)
		outerDst[i] = getClampedRoundedTessLevel(spacingMode, outerSrc[i]);
}

void getClampedRoundedQuadTessLevels (const SpacingMode spacingMode,
									  const float*		innerSrc,
									  const float*		outerSrc,
									  int*				innerDst,
									  int*				outerDst)
{
	for (int i = 0; i < 2; i++)
		innerDst[i] = getClampedRoundedTessLevel(spacingMode, innerSrc[i]);
	for (int i = 0; i < 4; i++)
		outerDst[i] = getClampedRoundedTessLevel(spacingMode, outerSrc[i]);
}

void getClampedRoundedIsolineTessLevels (const SpacingMode	spacingMode,
										 const float*		outerSrc,
										 int*				outerDst)
{
	outerDst[0] = getClampedRoundedTessLevel(SPACINGMODE_EQUAL,	outerSrc[0]);
	outerDst[1] = getClampedRoundedTessLevel(spacingMode,		outerSrc[1]);
}

int numOuterTessellationLevels (const TessPrimitiveType primType)
{
	switch (primType)
	{
		case TESSPRIMITIVETYPE_TRIANGLES:	return 3;
		case TESSPRIMITIVETYPE_QUADS:		return 4;
		case TESSPRIMITIVETYPE_ISOLINES:	return 2;
		default:
			DE_ASSERT(false);
			return 0;
	}
}

bool isPatchDiscarded (const TessPrimitiveType primitiveType, const float* outerLevels)
{
	const int numOuterLevels = numOuterTessellationLevels(primitiveType);
	for (int i = 0; i < numOuterLevels; i++)
		if (outerLevels[i] <= 0.0f)
			return true;
	return false;
}

std::string getTessellationLevelsString (const TessLevels& tessLevels, const TessPrimitiveType primitiveType)
{
	std::ostringstream str;
	switch (primitiveType)
	{
		case TESSPRIMITIVETYPE_ISOLINES:
			str << "inner: { }, "
				<< "outer: { " << tessLevels.outer[0] << ", " << tessLevels.outer[1] << " }";
			break;

		case TESSPRIMITIVETYPE_TRIANGLES:
			str << "inner: { " << tessLevels.inner[0] << " }, "
				<< "outer: { " << tessLevels.outer[0] << ", " << tessLevels.outer[1] << ", " << tessLevels.outer[2] << " }";
			break;

		case TESSPRIMITIVETYPE_QUADS:
			str << "inner: { " << tessLevels.inner[0] << ", " << tessLevels.inner[1] << " }, "
				<< "outer: { " << tessLevels.outer[0] << ", " << tessLevels.outer[1] << ", " << tessLevels.outer[2] << ", " << tessLevels.outer[3] << " }";
			break;

		default:
			DE_ASSERT(false);
	}

	return str.str();
}

//! Assumes array sizes inner[2] and outer[4].
std::string getTessellationLevelsString (const float* inner, const float* outer)
{
	const TessLevels tessLevels =
	{
		{ inner[0], inner[1] },
		{ outer[0], outer[1], outer[2], outer[3] }
	};
	return getTessellationLevelsString(tessLevels, TESSPRIMITIVETYPE_QUADS);
}

// \note The tessellation coordinates generated by this function could break some of the rules given in the spec
// (e.g. it may not exactly hold that u+v+w == 1.0f, or [uvw] + (1.0f-[uvw]) == 1.0f).
std::vector<tcu::Vec3> generateReferenceTriangleTessCoords (const SpacingMode	spacingMode,
															const int			inner,
															const int			outer0,
															const int			outer1,
															const int			outer2)
{
	std::vector<tcu::Vec3> tessCoords;

	if (inner == 1)
	{
		if (outer0 == 1 && outer1 == 1 && outer2 == 1)
		{
			tessCoords.push_back(tcu::Vec3(1.0f, 0.0f, 0.0f));
			tessCoords.push_back(tcu::Vec3(0.0f, 1.0f, 0.0f));
			tessCoords.push_back(tcu::Vec3(0.0f, 0.0f, 1.0f));
			return tessCoords;
		}
		else
			return generateReferenceTriangleTessCoords(spacingMode, spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
													   outer0, outer1, outer2);
	}
	else
	{
		for (int i = 0; i < outer0; i++) { const float v = (float)i / (float)outer0; tessCoords.push_back(tcu::Vec3(    0.0f,        v, 1.0f - v)); }
		for (int i = 0; i < outer1; i++) { const float v = (float)i / (float)outer1; tessCoords.push_back(tcu::Vec3(1.0f - v,     0.0f,        v)); }
		for (int i = 0; i < outer2; i++) { const float v = (float)i / (float)outer2; tessCoords.push_back(tcu::Vec3(       v, 1.0f - v,     0.0f)); }

		const int numInnerTriangles = inner/2;
		for (int innerTriangleNdx = 0; innerTriangleNdx < numInnerTriangles; innerTriangleNdx++)
		{
			const int curInnerTriangleLevel = inner - 2*(innerTriangleNdx+1);

			if (curInnerTriangleLevel == 0)
				tessCoords.push_back(tcu::Vec3(1.0f/3.0f));
			else
			{
				const float		minUVW		= (float)(2 * (innerTriangleNdx + 1)) / (float)(3 * inner);
				const float		maxUVW		= 1.0f - 2.0f*minUVW;
				const tcu::Vec3	corners[3]	=
				{
					tcu::Vec3(maxUVW, minUVW, minUVW),
					tcu::Vec3(minUVW, maxUVW, minUVW),
					tcu::Vec3(minUVW, minUVW, maxUVW)
				};

				for (int i = 0; i < curInnerTriangleLevel; i++)
				{
					const float f = (float)i / (float)curInnerTriangleLevel;
					for (int j = 0; j < 3; j++)
						tessCoords.push_back((1.0f - f)*corners[j] + f*corners[(j+1)%3]);
				}
			}
		}

		return tessCoords;
	}
}

// \note The tessellation coordinates generated by this function could break some of the rules given in the spec
// (e.g. it may not exactly hold that [uv] + (1.0f-[uv]) == 1.0f).
std::vector<tcu::Vec3> generateReferenceQuadTessCoords (const SpacingMode	spacingMode,
														const int			inner0,
														const int			inner1,
														const int			outer0,
														const int			outer1,
														const int			outer2,
														const int			outer3)
{
	std::vector<tcu::Vec3> tessCoords;

	if (inner0 == 1 || inner1 == 1)
	{
		if (inner0 == 1 && inner1 == 1 && outer0 == 1 && outer1 == 1 && outer2 == 1 && outer3 == 1)
		{
			tessCoords.push_back(tcu::Vec3(0.0f, 0.0f, 0.0f));
			tessCoords.push_back(tcu::Vec3(1.0f, 0.0f, 0.0f));
			tessCoords.push_back(tcu::Vec3(0.0f, 1.0f, 0.0f));
			tessCoords.push_back(tcu::Vec3(1.0f, 1.0f, 0.0f));
			return tessCoords;
		}
		else
			return generateReferenceQuadTessCoords(spacingMode, inner0 > 1 ? inner0 : spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
																inner1 > 1 ? inner1 : spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
																outer0, outer1, outer2, outer3);
	}
	else
	{
		for (int i = 0; i < outer0; i++) { const float v = (float)i / (float)outer0; tessCoords.push_back(tcu::Vec3(    0.0f,        v, 0.0f)); }
		for (int i = 0; i < outer1; i++) { const float v = (float)i / (float)outer1; tessCoords.push_back(tcu::Vec3(1.0f - v,     0.0f, 0.0f)); }
		for (int i = 0; i < outer2; i++) { const float v = (float)i / (float)outer2; tessCoords.push_back(tcu::Vec3(    1.0f, 1.0f - v, 0.0f)); }
		for (int i = 0; i < outer3; i++) { const float v = (float)i / (float)outer3; tessCoords.push_back(tcu::Vec3(       v,     1.0f, 0.0f)); }

		for (int innerVtxY = 0; innerVtxY < inner1-1; innerVtxY++)
		for (int innerVtxX = 0; innerVtxX < inner0-1; innerVtxX++)
			tessCoords.push_back(tcu::Vec3((float)(innerVtxX + 1) / (float)inner0,
										   (float)(innerVtxY + 1) / (float)inner1,
										   0.0f));

		return tessCoords;
	}
}

// \note The tessellation coordinates generated by this function could break some of the rules given in the spec
// (e.g. it may not exactly hold that [uv] + (1.0f-[uv]) == 1.0f).
std::vector<tcu::Vec3> generateReferenceIsolineTessCoords (const int outer0, const int outer1)
{
	std::vector<tcu::Vec3> tessCoords;

	for (int y = 0; y < outer0;   y++)
	for (int x = 0; x < outer1+1; x++)
		tessCoords.push_back(tcu::Vec3((float)x / (float)outer1,
									   (float)y / (float)outer0,
									   0.0f));

	return tessCoords;
}

static int referencePointModePrimitiveCount (const TessPrimitiveType primitiveType, const SpacingMode spacingMode, const float* innerLevels, const float* outerLevels)
{
	if (isPatchDiscarded(primitiveType, outerLevels))
		return 0;

	switch (primitiveType)
	{
		case TESSPRIMITIVETYPE_TRIANGLES:
		{
			int inner;
			int outer[3];
			getClampedRoundedTriangleTessLevels(spacingMode, innerLevels, outerLevels, &inner, &outer[0]);
			return static_cast<int>(generateReferenceTriangleTessCoords(spacingMode, inner, outer[0], outer[1], outer[2]).size());
		}

		case TESSPRIMITIVETYPE_QUADS:
		{
			int inner[2];
			int outer[4];
			getClampedRoundedQuadTessLevels(spacingMode, innerLevels, outerLevels, &inner[0], &outer[0]);
			return static_cast<int>(generateReferenceQuadTessCoords(spacingMode, inner[0], inner[1], outer[0], outer[1], outer[2], outer[3]).size());
		}

		case TESSPRIMITIVETYPE_ISOLINES:
		{
			int outer[2];
			getClampedRoundedIsolineTessLevels(spacingMode, &outerLevels[0], &outer[0]);
			return static_cast<int>(generateReferenceIsolineTessCoords(outer[0], outer[1]).size());
		}

		default:
			DE_ASSERT(false);
			return 0;
	}
}

static int referenceTriangleNonPointModePrimitiveCount (const SpacingMode spacingMode, const int inner, const int outer0, const int outer1, const int outer2)
{
	if (inner == 1)
	{
		if (outer0 == 1 && outer1 == 1 && outer2 == 1)
			return 1;
		else
			return referenceTriangleNonPointModePrimitiveCount(spacingMode, spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
																			outer0, outer1, outer2);
	}
	else
	{
		int result = outer0 + outer1 + outer2;

		const int numInnerTriangles = inner/2;
		for (int innerTriangleNdx = 0; innerTriangleNdx < numInnerTriangles; innerTriangleNdx++)
		{
			const int curInnerTriangleLevel = inner - 2*(innerTriangleNdx+1);

			if (curInnerTriangleLevel == 1)
				result += 4;
			else
				result += 2*3*curInnerTriangleLevel;
		}

		return result;
	}
}

static int referenceQuadNonPointModePrimitiveCount (const SpacingMode spacingMode, const int inner0, const int inner1, const int outer0, const int outer1, const int outer2, const int outer3)
{
	if (inner0 == 1 || inner1 == 1)
	{
		if (inner0 == 1 && inner1 == 1 && outer0 == 1 && outer1 == 1 && outer2 == 1 && outer3 == 1)
			return 2;
		else
			return referenceQuadNonPointModePrimitiveCount(spacingMode, inner0 > 1 ? inner0 : spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
																		inner1 > 1 ? inner1 : spacingMode == SPACINGMODE_FRACTIONAL_ODD ? 3 : 2,
																		outer0, outer1, outer2, outer3);
	}
	else
		return 2*(inner0-2)*(inner1-2) + 2*(inner0-2) + 2*(inner1-2) + outer0+outer1+outer2+outer3;
}

static inline int referenceIsolineNonPointModePrimitiveCount (const int outer0, const int outer1)
{
	return outer0*outer1;
}

static int referenceNonPointModePrimitiveCount (const TessPrimitiveType primitiveType, const SpacingMode spacingMode, const float* innerLevels, const float* outerLevels)
{
	if (isPatchDiscarded(primitiveType, outerLevels))
		return 0;

	switch (primitiveType)
	{
		case TESSPRIMITIVETYPE_TRIANGLES:
		{
			int inner;
			int outer[3];
			getClampedRoundedTriangleTessLevels(spacingMode, innerLevels, outerLevels, &inner, &outer[0]);
			return referenceTriangleNonPointModePrimitiveCount(spacingMode, inner, outer[0], outer[1], outer[2]);
		}

		case TESSPRIMITIVETYPE_QUADS:
		{
			int inner[2];
			int outer[4];
			getClampedRoundedQuadTessLevels(spacingMode, innerLevels, outerLevels, &inner[0], &outer[0]);
			return referenceQuadNonPointModePrimitiveCount(spacingMode, inner[0], inner[1], outer[0], outer[1], outer[2], outer[3]);
		}

		case TESSPRIMITIVETYPE_ISOLINES:
		{
			int outer[2];
			getClampedRoundedIsolineTessLevels(spacingMode, &outerLevels[0], &outer[0]);
			return referenceIsolineNonPointModePrimitiveCount(outer[0], outer[1]);
		}

		default:
			DE_ASSERT(false);
			return 0;
	}
}

int numVerticesPerPrimitive (const TessPrimitiveType primitiveType, const bool usePointMode)
{
	if (usePointMode)
		return 1;

	switch (primitiveType)
	{
		case TESSPRIMITIVETYPE_TRIANGLES:	return 3;
		case TESSPRIMITIVETYPE_QUADS:		return 3;  // quads are composed of two triangles
		case TESSPRIMITIVETYPE_ISOLINES:	return 2;
		default:
			DE_ASSERT(false);
			return 0;
	}
}

int referencePrimitiveCount (const TessPrimitiveType primitiveType, const SpacingMode spacingMode, const bool usePointMode, const float* innerLevels, const float* outerLevels)
{
	return usePointMode ? referencePointModePrimitiveCount		(primitiveType, spacingMode, innerLevels, outerLevels)
						: referenceNonPointModePrimitiveCount	(primitiveType, spacingMode, innerLevels, outerLevels);
}

//! In point mode this should return the number of unique vertices, while in non-point mode the maximum theoretical number of verticies.
//! Actual implementation will likely return a much smaller number because the shader isn't required to be run for duplicate coordinates.
int referenceVertexCount (const TessPrimitiveType primitiveType, const SpacingMode spacingMode, const bool usePointMode, const float* innerLevels, const float* outerLevels)
{
	return referencePrimitiveCount(primitiveType, spacingMode, usePointMode, innerLevels, outerLevels)
		   * numVerticesPerPrimitive(primitiveType, usePointMode);
}

void requireFeatures (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const FeatureFlags flags)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if (((flags & FEATURE_TESSELLATION_SHADER) != 0) && !features.tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader not supported");

	if (((flags & FEATURE_GEOMETRY_SHADER) != 0) && !features.geometryShader)
		throw tcu::NotSupportedError("Geometry shader not supported");

	if (((flags & FEATURE_SHADER_FLOAT_64) != 0) && !features.shaderFloat64)
		throw tcu::NotSupportedError("Double-precision floats not supported");

	if (((flags & FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS) != 0) && !features.vertexPipelineStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in vertex pipeline");

	if (((flags & FEATURE_FRAGMENT_STORES_AND_ATOMICS) != 0) && !features.fragmentStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in fragment shader");

	if (((flags & FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE) != 0) && !features.shaderTessellationAndGeometryPointSize)
		throw tcu::NotSupportedError("Tessellation and geometry shaders don't support PointSize built-in");
}

} // tessellation
} // vkt
