/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups Tests Utils
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsTestsUtils.hpp"
#include "vkRayTracingUtil.hpp"
#include "deFloat16.h"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{

deUint32 getMaxWidth ()
{
	return 1024u;
}

deUint32 getNextWidth (const deUint32 width)
{
	if (width < 128)
	{
		// This ensures we test every value up to 128 (the max subgroup size).
		return width + 1;
	}
	else
	{
		// And once we hit 128 we increment to only power of 2's to reduce testing time.
		return width * 2;
	}
}

deUint32 getFormatSizeInBytes (const VkFormat format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			return 0;
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_UINT:
			return static_cast<deUint32>(sizeof(deInt8));
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_UINT:
			return static_cast<deUint32>(sizeof(deInt8) * 2);
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_UINT:
			return static_cast<deUint32>(sizeof(deInt8) * 4);
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt16));
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt16) * 2);
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt16) * 4);
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt32));
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt64));
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt64) * 2);
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64G64B64A64_UINT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return static_cast<deUint32>(sizeof(deInt64) * 4);
		// The below formats are used to represent bool and bvec* types. These
		// types are passed to the shader as int and ivec* types, before the
		// calculations are done as booleans. We need a distinct type here so
		// that the shader generators can switch on it and generate the correct
		// shader source for testing.
		case VK_FORMAT_R8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32));
		case VK_FORMAT_R8G8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
	}
}

deUint32 getElementSizeInBytes (const VkFormat									format,
								const subgroups::SSBOData::InputDataLayoutType	layout)
{
	const deUint32 bytes = getFormatSizeInBytes(format);

	if (layout == subgroups::SSBOData::LayoutStd140)
		return bytes < 16 ? 16 : bytes;
	else
		return bytes;
}

Move<VkRenderPass> makeRenderPass (Context& context, VkFormat format)
{
	const VkAttachmentReference		colorReference			=
	{
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	const VkSubpassDescription		subpassDescription		=
	{
		0u,									//  VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	//  VkPipelineBindPoint				pipelineBindPoint;
		0,									//  deUint32						inputAttachmentCount;
		DE_NULL,							//  const VkAttachmentReference*	pInputAttachments;
		1,									//  deUint32						colorAttachmentCount;
		&colorReference,					//  const VkAttachmentReference*	pColorAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pResolveAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pDepthStencilAttachment;
		0,									//  deUint32						preserveAttachmentCount;
		DE_NULL								//  const deUint32*					pPreserveAttachments;
	};
	const VkSubpassDependency		subpassDependencies[2]	=
	{
		{
			VK_SUBPASS_EXTERNAL,															//  deUint32				srcSubpass;
			0u,																				//  deUint32				dstSubpass;
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,											//  VkPipelineStageFlags	srcStageMask;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,									//  VkPipelineStageFlags	dstStageMask;
			VK_ACCESS_MEMORY_READ_BIT,														//  VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		//  VkAccessFlags			dstAccessMask;
			VK_DEPENDENCY_BY_REGION_BIT														//  VkDependencyFlags		dependencyFlags;
		},
		{
			0u,																				//  deUint32				srcSubpass;
			VK_SUBPASS_EXTERNAL,															//  deUint32				dstSubpass;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,									//  VkPipelineStageFlags	srcStageMask;
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,											//  VkPipelineStageFlags	dstStageMask;
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		//  VkAccessFlags			srcAccessMask;
			VK_ACCESS_MEMORY_READ_BIT,														//  VkAccessFlags			dstAccessMask;
			VK_DEPENDENCY_BY_REGION_BIT														//  VkDependencyFlags		dependencyFlags;
		},
	};
	const VkAttachmentDescription	attachmentDescription	=
	{
		0u,											//  VkAttachmentDescriptionFlags	flags;
		format,										//  VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,						//  VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				//  VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				//  VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			//  VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					//  VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL		//  VkImageLayout					finalLayout;
	};
	const VkRenderPassCreateInfo	renderPassCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,									//  const void*						pNext;
		0u,											//  VkRenderPassCreateFlags			flags;
		1,											//  deUint32						attachmentCount;
		&attachmentDescription,						//  const VkAttachmentDescription*	pAttachments;
		1,											//  deUint32						subpassCount;
		&subpassDescription,						//  const VkSubpassDescription*		pSubpasses;
		2,											//  deUint32						dependencyCount;
		subpassDependencies							//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(context.getDeviceInterface(), context.getDevice(), &renderPassCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&							vk,
									   const VkDevice									device,
									   const VkPipelineLayout							pipelineLayout,
									   const VkShaderModule								vertexShaderModule,
									   const VkShaderModule								tessellationControlShaderModule,
									   const VkShaderModule								tessellationEvalShaderModule,
									   const VkShaderModule								geometryShaderModule,
									   const VkShaderModule								fragmentShaderModule,
									   const VkRenderPass								renderPass,
									   const std::vector<VkViewport>&					viewports,
									   const std::vector<VkRect2D>&						scissors,
									   const VkPrimitiveTopology						topology,
									   const deUint32									subpass,
									   const deUint32									patchControlPoints,
									   const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo,
									   const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo,
									   const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo,
									   const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo,
									   const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo,
									   const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo,
									   const deUint32									vertexShaderStageCreateFlags,
									   const deUint32									tessellationControlShaderStageCreateFlags,
									   const deUint32									tessellationEvalShaderStageCreateFlags,
									   const deUint32									geometryShaderStageCreateFlags,
									   const deUint32									fragmentShaderStageCreateFlags,
									   const deUint32									requiredSubgroupSize[5])
{
	const VkBool32									disableRasterization				= (fragmentShaderModule == DE_NULL);
	const bool										hasTessellation						= (tessellationControlShaderModule != DE_NULL || tessellationEvalShaderModule != DE_NULL);

	VkPipelineShaderStageCreateInfo					stageCreateInfo						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType
		DE_NULL,												// const void*                         pNext
		0u,														// VkPipelineShaderStageCreateFlags    flags
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage
		DE_NULL,												// VkShaderModule                      module
		"main",													// const char*                         pName
		DE_NULL													// const VkSpecializationInfo*         pSpecializationInfo
	};

	std::vector<VkPipelineShaderStageCreateInfo>	pipelineShaderStageParams;

	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT requiredSubgroupSizeCreateInfo[5] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[0] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[1] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[2] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[3] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[4] : 0u,
		},
	};

	{
		stageCreateInfo.pNext	= (requiredSubgroupSizeCreateInfo[0].requiredSubgroupSize != 0u) ? &requiredSubgroupSizeCreateInfo[0] : DE_NULL;
		stageCreateInfo.flags	= vertexShaderStageCreateFlags;
		stageCreateInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
		stageCreateInfo.module	= vertexShaderModule;
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (tessellationControlShaderModule != DE_NULL)
	{
		stageCreateInfo.pNext	= (requiredSubgroupSizeCreateInfo[1].requiredSubgroupSize != 0u) ? &requiredSubgroupSizeCreateInfo[1] : DE_NULL;
		stageCreateInfo.flags	= tessellationControlShaderStageCreateFlags;
		stageCreateInfo.stage	= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		stageCreateInfo.module	= tessellationControlShaderModule;
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (tessellationEvalShaderModule != DE_NULL)
	{
		stageCreateInfo.pNext	= (requiredSubgroupSize != DE_NULL && requiredSubgroupSizeCreateInfo[2].requiredSubgroupSize != 0u) ? &requiredSubgroupSizeCreateInfo[2] : DE_NULL;
		stageCreateInfo.flags	= tessellationEvalShaderStageCreateFlags;
		stageCreateInfo.stage	= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		stageCreateInfo.module	= tessellationEvalShaderModule;
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (geometryShaderModule != DE_NULL)
	{
		stageCreateInfo.pNext	= (requiredSubgroupSizeCreateInfo[3].requiredSubgroupSize != 0u) ? &requiredSubgroupSizeCreateInfo[3] : DE_NULL;
		stageCreateInfo.flags	= geometryShaderStageCreateFlags;
		stageCreateInfo.stage	= VK_SHADER_STAGE_GEOMETRY_BIT;
		stageCreateInfo.module	= geometryShaderModule;
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	if (fragmentShaderModule != DE_NULL)
	{
		stageCreateInfo.pNext	= (requiredSubgroupSizeCreateInfo[4].requiredSubgroupSize != 0u) ? &requiredSubgroupSizeCreateInfo[4] : DE_NULL;
		stageCreateInfo.flags	= fragmentShaderStageCreateFlags;
		stageCreateInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
		stageCreateInfo.module	= fragmentShaderModule;
		pipelineShaderStageParams.push_back(stageCreateInfo);
	}

	const VkVertexInputBindingDescription			vertexInputBindingDescription		=
	{
		0u,								// deUint32             binding
		sizeof(tcu::Vec4),				// deUint32             stride
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate    inputRate
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescription		=
	{
		0u,								// deUint32    location
		0u,								// deUint32    binding
		VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat    format
		0u								// deUint32    offset
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfoDefault	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,													// const void*                                 pNext
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags
		1u,															// deUint32                                    vertexBindingDescriptionCount
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		1u,															// deUint32                                    vertexAttributeDescriptionCount
		&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,														// const void*                                pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags    flags
		topology,														// VkPrimitiveTopology                        topology
		VK_FALSE														// VkBool32                                   primitiveRestartEnable
	};

	const VkPipelineTessellationStateCreateInfo		tessStateCreateInfo					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	// VkStructureType                           sType
		DE_NULL,													// const void*                               pNext
		0u,															// VkPipelineTessellationStateCreateFlags    flags
		patchControlPoints											// deUint32                                  patchControlPoints
	};

	const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,												// const void*                                 pNext
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags
		viewports.empty() ? 1u : (deUint32)viewports.size(),	// deUint32                                    viewportCount
		viewports.empty() ? DE_NULL : &viewports[0],			// const VkViewport*                           pViewports
		viewports.empty() ? 1u : (deUint32)scissors.size(),		// deUint32                                    scissorCount
		scissors.empty() ? DE_NULL : &scissors[0]				// const VkRect2D*                             pScissors
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfoDefault	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,													// const void*                                pNext
		0u,															// VkPipelineRasterizationStateCreateFlags    flags
		VK_FALSE,													// VkBool32                                   depthClampEnable
		disableRasterization,										// VkBool32                                   rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags                            cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace                                frontFace
		VK_FALSE,													// VkBool32                                   depthBiasEnable
		0.0f,														// float                                      depthBiasConstantFactor
		0.0f,														// float                                      depthBiasClamp
		0.0f,														// float                                      depthBiasSlopeFactor
		1.0f														// float                                      lineWidth
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfoDefault	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType                          sType
		DE_NULL,													// const void*                              pNext
		0u,															// VkPipelineMultisampleStateCreateFlags    flags
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits                    rasterizationSamples
		VK_FALSE,													// VkBool32                                 sampleShadingEnable
		1.0f,														// float                                    minSampleShading
		DE_NULL,													// const VkSampleMask*                      pSampleMask
		VK_FALSE,													// VkBool32                                 alphaToCoverageEnable
		VK_FALSE													// VkBool32                                 alphaToOneEnable
	};

	const VkStencilOpState							stencilOpState						=
	{
		VK_STENCIL_OP_KEEP,		// VkStencilOp    failOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp    passOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp    depthFailOp
		VK_COMPARE_OP_NEVER,	// VkCompareOp    compareOp
		0,						// deUint32       compareMask
		0,						// deUint32       writeMask
		0						// deUint32       reference
	};

	const VkPipelineDepthStencilStateCreateInfo		depthStencilStateCreateInfoDefault	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		DE_NULL,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		VK_FALSE,													// VkBool32                                 depthTestEnable
		VK_FALSE,													// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		VK_FALSE,													// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f,														// float                                    maxDepthBounds
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState			=
	{
		VK_FALSE,					// VkBool32                 blendEnable
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags    colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfoDefault	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType
		DE_NULL,													// const void*                                   pNext
		0u,															// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,													// VkBool32                                      logicOpEnable
		VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp
		1u,															// deUint32                                      attachmentCount
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float                                         blendConstants[4]
	};

	std::vector<VkDynamicState>						dynamicStates;

	if (viewports.empty())
		dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	if (scissors.empty())
		dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

	const VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfoDefault		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType
		DE_NULL,												// const void*                          pNext
		0u,														// VkPipelineDynamicStateCreateFlags    flags
		(deUint32)dynamicStates.size(),							// deUint32                             dynamicStateCount
		dynamicStates.empty() ? DE_NULL : &dynamicStates[0]		// const VkDynamicState*                pDynamicStates
	};

	const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfoDefaultPtr	= dynamicStates.empty() ? DE_NULL : &dynamicStateCreateInfoDefault;

	const VkGraphicsPipelineCreateInfo				pipelineCreateInfo					=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,														// VkStructureType                                  sType
		DE_NULL,																								// const void*                                      pNext
		0u,																										// VkPipelineCreateFlags                            flags
		(deUint32)pipelineShaderStageParams.size(),																// deUint32                                         stageCount
		&pipelineShaderStageParams[0],																			// const VkPipelineShaderStageCreateInfo*           pStages
		vertexInputStateCreateInfo ? vertexInputStateCreateInfo : &vertexInputStateCreateInfoDefault,			// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
		&inputAssemblyStateCreateInfo,																			// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
		hasTessellation ? &tessStateCreateInfo : DE_NULL,														// const VkPipelineTessellationStateCreateInfo*     pTessellationState
		&viewportStateCreateInfo,																				// const VkPipelineViewportStateCreateInfo*         pViewportState
		rasterizationStateCreateInfo ? rasterizationStateCreateInfo : &rasterizationStateCreateInfoDefault,		// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
		multisampleStateCreateInfo ? multisampleStateCreateInfo: &multisampleStateCreateInfoDefault,			// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
		depthStencilStateCreateInfo ? depthStencilStateCreateInfo : &depthStencilStateCreateInfoDefault,		// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
		colorBlendStateCreateInfo ? colorBlendStateCreateInfo : &colorBlendStateCreateInfoDefault,				// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
		dynamicStateCreateInfo ? dynamicStateCreateInfo : dynamicStateCreateInfoDefaultPtr,						// const VkPipelineDynamicStateCreateInfo*          pDynamicState
		pipelineLayout,																							// VkPipelineLayout                                 layout
		renderPass,																								// VkRenderPass                                     renderPass
		subpass,																								// deUint32                                         subpass
		DE_NULL,																								// VkPipeline                                       basePipelineHandle
		0																										// deInt32                                          basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline (Context&									context,
									   const VkPipelineLayout					pipelineLayout,
									   const VkShaderStageFlags					stages,
									   const VkShaderModule						vertexShaderModule,
									   const VkShaderModule						fragmentShaderModule,
									   const VkShaderModule						geometryShaderModule,
									   const VkShaderModule						tessellationControlModule,
									   const VkShaderModule						tessellationEvaluationModule,
									   const VkRenderPass						renderPass,
									   const VkPrimitiveTopology				topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									   const VkVertexInputBindingDescription*	vertexInputBindingDescription = DE_NULL,
									   const VkVertexInputAttributeDescription*	vertexInputAttributeDescriptions = DE_NULL,
									   const bool								frameBufferTests = false,
									   const vk::VkFormat						attachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT,
									   const deUint32							vertexShaderStageCreateFlags = 0u,
									   const deUint32							tessellationControlShaderStageCreateFlags = 0u,
									   const deUint32							tessellationEvalShaderStageCreateFlags = 0u,
									   const deUint32							geometryShaderStageCreateFlags = 0u,
									   const deUint32							fragmentShaderStageCreateFlags = 0u,
									   const deUint32							requiredSubgroupSize[5] = DE_NULL)
{
	const std::vector<VkViewport>				noViewports;
	const std::vector<VkRect2D>					noScissors;
	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineVertexInputStateCreateFlags		flags;
		vertexInputBindingDescription == DE_NULL ? 0u : 1u,			// deUint32										vertexBindingDescriptionCount;
		vertexInputBindingDescription,								// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		vertexInputAttributeDescriptions == DE_NULL ? 0u : 1u,		// deUint32										vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,							// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
	};
	const deUint32								numChannels					= getNumUsedChannels(mapVkFormat(attachmentFormat).order);
	const VkColorComponentFlags					colorComponent				= numChannels == 1 ? VK_COLOR_COMPONENT_R_BIT :
																			  numChannels == 2 ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT :
																			  numChannels == 3 ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT :
																			  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	const VkPipelineColorBlendAttachmentState	colorBlendAttachmentState	=
	{
		VK_FALSE,				//  VkBool32				blendEnable;
		VK_BLEND_FACTOR_ZERO,	//  VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//  VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		//  VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ZERO,	//  VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//  VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		//  VkBlendOp				alphaBlendOp;
		colorComponent			//  VkColorComponentFlags	colorWriteMask;
	};
	const VkPipelineColorBlendStateCreateInfo	colorBlendStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//  VkStructureType								sType;
		DE_NULL,													//  const void*									pNext;
		0u,															//  VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//  VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//  VkLogicOp									logicOp;
		1,															//  deUint32									attachmentCount;
		&colorBlendAttachmentState,									//  const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f }									//  float										blendConstants[4];
	};
	const deUint32								patchControlPoints			= (VK_SHADER_STAGE_FRAGMENT_BIT & stages && frameBufferTests) ? 2u : 1u;

	return makeGraphicsPipeline(context.getDeviceInterface(),	// const DeviceInterface&                        vk
								context.getDevice(),			// const VkDevice                                device
								pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
								vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
								tessellationControlModule,		// const VkShaderModule                          tessellationControlShaderModule
								tessellationEvaluationModule,	// const VkShaderModule                          tessellationEvalShaderModule
								geometryShaderModule,			// const VkShaderModule                          geometryShaderModule
								fragmentShaderModule,			// const VkShaderModule                          fragmentShaderModule
								renderPass,						// const VkRenderPass                            renderPass
								noViewports,					// const std::vector<VkViewport>&                viewports
								noScissors,						// const std::vector<VkRect2D>&                  scissors
								topology,						// const VkPrimitiveTopology                     topology
								0u,								// const deUint32                                subpass
								patchControlPoints,				// const deUint32                                patchControlPoints
								&vertexInputStateCreateInfo,	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,						// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								DE_NULL,						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								DE_NULL,						// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								&colorBlendStateCreateInfo,		// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
								DE_NULL,						// const VkPipelineDynamicStateCreateInfo*
								vertexShaderStageCreateFlags,	// const deUint32								 vertexShaderStageCreateFlags,
								tessellationControlShaderStageCreateFlags,	// const deUint32					 tessellationControlShaderStageCreateFlags
								tessellationEvalShaderStageCreateFlags,		// const deUint32					 tessellationEvalShaderStageCreateFlags
								geometryShaderStageCreateFlags,	// const deUint32								 geometryShaderStageCreateFlags
								fragmentShaderStageCreateFlags,	// const deUint32								 fragmentShaderStageCreateFlags
								requiredSubgroupSize);			// const deUint32								 requiredSubgroupSize[5]
}

Move<VkCommandBuffer> makeCommandBuffer (Context& context, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo bufferAllocateParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		commandPool,									// VkCommandPool		commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel	level;
		1u,												// deUint32				bufferCount;
	};
	return allocateCommandBuffer(context.getDeviceInterface(),
								 context.getDevice(), &bufferAllocateParams);
}

struct Buffer;
struct Image;

struct BufferOrImage
{
	bool isImage() const
	{
		return m_isImage;
	}

	Buffer* getAsBuffer()
	{
		if (m_isImage) DE_FATAL("Trying to get a buffer as an image!");
		return reinterpret_cast<Buffer* >(this);
	}

	Image* getAsImage()
	{
		if (!m_isImage) DE_FATAL("Trying to get an image as a buffer!");
		return reinterpret_cast<Image*>(this);
	}

	virtual VkDescriptorType getType() const
	{
		if (m_isImage)
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		else
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
	}

	Allocation& getAllocation() const
	{
		return *m_allocation;
	}

	virtual ~BufferOrImage() {}

protected:
	explicit BufferOrImage(bool image) : m_isImage(image) {}

	bool m_isImage;
	de::details::MovePtr<Allocation> m_allocation;
};

struct Buffer : public BufferOrImage
{
	explicit Buffer (Context& context, VkDeviceSize sizeInBytes, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		: BufferOrImage		(false)
		, m_sizeInBytes		(sizeInBytes)
		, m_usage			(usage)
	{
		const DeviceInterface&			vkd					= context.getDeviceInterface();
		const VkDevice					device				= context.getDevice();

		const vk::VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			0u,
			m_sizeInBytes,
			m_usage,
			VK_SHARING_MODE_EXCLUSIVE,
			0u,
			DE_NULL,
		};
		m_buffer		= createBuffer(vkd, device, &bufferCreateInfo);

		VkMemoryRequirements			req					= getBufferMemoryRequirements(vkd, device, *m_buffer);

		m_allocation	= context.getDefaultAllocator().allocate(req, MemoryRequirement::HostVisible);
		VK_CHECK(vkd.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
	}

	virtual VkDescriptorType getType() const
	{
		if (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT == m_usage)
		{
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		}
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	VkBuffer getBuffer () const
	{
		return *m_buffer;
	}

	const VkBuffer* getBufferPtr () const
	{
		return &(*m_buffer);
	}

	VkDeviceSize getSize () const
	{
		return m_sizeInBytes;
	}

private:
	Move<VkBuffer>				m_buffer;
	VkDeviceSize				m_sizeInBytes;
	const VkBufferUsageFlags	m_usage;
};

struct Image : public BufferOrImage
{
	explicit Image (Context& context, deUint32 width, deUint32 height, VkFormat format, VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT)
		: BufferOrImage(true)
	{
		const DeviceInterface&			vk					= context.getDeviceInterface();
		const VkDevice					device				= context.getDevice();
		const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

		const VkImageCreateInfo			imageCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType			sType;
			DE_NULL,								//  const void*				pNext;
			0,										//  VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//  VkImageType				imageType;
			format,									//  VkFormat				format;
			{width, height, 1},						//  VkExtent3D				extent;
			1,										//  deUint32				mipLevels;
			1,										//  deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling			tiling;
			usage,									//  VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode			sharingMode;
			0u,										//  deUint32				queueFamilyIndexCount;
			DE_NULL,								//  const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED				//  VkImageLayout			initialLayout;
		};

		const VkComponentMapping		componentMapping	=
		{
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
		};

		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask
			0u,							//deUint32				baseMipLevel
			1u,							//deUint32				levelCount
			0u,							//deUint32				baseArrayLayer
			1u							//deUint32				layerCount
		};

		const VkSamplerCreateInfo		samplerCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		//  VkStructureType			sType;
			DE_NULL,									//  const void*				pNext;
			0u,											//  VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,							//  VkFilter				magFilter;
			VK_FILTER_NEAREST,							//  VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				//  VkSamplerMipmapMode		mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeW;
			0.0f,										//  float					mipLodBias;
			VK_FALSE,									//  VkBool32				anisotropyEnable;
			1.0f,										//  float					maxAnisotropy;
			DE_FALSE,									//  VkBool32				compareEnable;
			VK_COMPARE_OP_ALWAYS,						//  VkCompareOp				compareOp;
			0.0f,										//  float					minLod;
			0.0f,										//  float					maxLod;
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	//  VkBorderColor			borderColor;
			VK_FALSE,									//  VkBool32				unnormalizedCoordinates;
		};

		m_image			= createImage(vk, device, &imageCreateInfo);

		VkMemoryRequirements			req					= getImageMemoryRequirements(vk, device, *m_image);

		req.size		*= 2;
		m_allocation	= context.getDefaultAllocator().allocate(req, MemoryRequirement::Any);

		VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));

		const VkImageViewCreateInfo		imageViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//  VkStructureType			sType;
			DE_NULL,									//  const void*				pNext;
			0,											//  VkImageViewCreateFlags	flags;
			*m_image,									//  VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,						//  VkImageViewType			viewType;
			imageCreateInfo.format,						//  VkFormat				format;
			componentMapping,							//  VkComponentMapping		components;
			subresourceRange							//  VkImageSubresourceRange	subresourceRange;
		};

		m_imageView		= createImageView(vk, device, &imageViewCreateInfo);
		m_sampler		= createSampler(vk, device, &samplerCreateInfo);

		// Transition input image layouts
		{
			const Unique<VkCommandPool>		cmdPool			(makeCommandPool(vk, device, queueFamilyIndex));
			const Unique<VkCommandBuffer>	cmdBuffer		(makeCommandBuffer(context, *cmdPool));

			beginCommandBuffer(vk, *cmdBuffer);

			const VkImageMemoryBarrier		imageBarrier	= makeImageMemoryBarrier((VkAccessFlags)0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																	VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *m_image, subresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &imageBarrier);

			endCommandBuffer(vk, *cmdBuffer);
			submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);
		}
	}

	VkImage getImage () const
	{
		return *m_image;
	}

	VkImageView getImageView () const
	{
		return *m_imageView;
	}

	VkSampler getSampler () const
	{
		return *m_sampler;
	}

private:
	Move<VkImage>		m_image;
	Move<VkImageView>	m_imageView;
	Move<VkSampler>		m_sampler;
};
}

deUint32 vkt::subgroups::getStagesCount (const VkShaderStageFlags shaderStages)
{
	const deUint32	stageCount	= isAllRayTracingStages(shaderStages) ? 6
								: isAllGraphicsStages(shaderStages)   ? 4
								: isAllComputeStages(shaderStages)    ? 1
								: 0;

	DE_ASSERT(stageCount != 0);

	return stageCount;
}

std::string vkt::subgroups::getSharedMemoryBallotHelper ()
{
	return	"shared uvec4 superSecretComputeShaderHelper[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
			"uvec4 sharedMemoryBallot(bool vote)\n"
			"{\n"
			"  uint groupOffset = gl_SubgroupID;\n"
			"  // One invocation in the group 0's the whole group's data\n"
			"  if (subgroupElect())\n"
			"  {\n"
			"    superSecretComputeShaderHelper[groupOffset] = uvec4(0);\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  if (vote)\n"
			"  {\n"
			"    const highp uint invocationId = gl_SubgroupInvocationID % 32;\n"
			"    const highp uint bitToSet = 1u << invocationId;\n"
			"    switch (gl_SubgroupInvocationID / 32)\n"
			"    {\n"
			"    case 0: atomicOr(superSecretComputeShaderHelper[groupOffset].x, bitToSet); break;\n"
			"    case 1: atomicOr(superSecretComputeShaderHelper[groupOffset].y, bitToSet); break;\n"
			"    case 2: atomicOr(superSecretComputeShaderHelper[groupOffset].z, bitToSet); break;\n"
			"    case 3: atomicOr(superSecretComputeShaderHelper[groupOffset].w, bitToSet); break;\n"
			"    }\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  return superSecretComputeShaderHelper[groupOffset];\n"
			"}\n";
}

std::string vkt::subgroups::getSharedMemoryBallotHelperARB ()
{
	return	"shared uvec4 superSecretComputeShaderHelper[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
			"uint64_t sharedMemoryBallot(bool vote)\n"
			"{\n"
			"  uint groupOffset = gl_SubgroupID;\n"
			"  // One invocation in the group 0's the whole group's data\n"
			"  if (subgroupElect())\n"
			"  {\n"
			"    superSecretComputeShaderHelper[groupOffset] = uvec4(0);\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  if (vote)\n"
			"  {\n"
			"    const highp uint invocationId = gl_SubgroupInvocationID % 32;\n"
			"    const highp uint bitToSet = 1u << invocationId;\n"
			"    switch (gl_SubgroupInvocationID / 32)\n"
			"    {\n"
			"    case 0: atomicOr(superSecretComputeShaderHelper[groupOffset].x, bitToSet); break;\n"
			"    case 1: atomicOr(superSecretComputeShaderHelper[groupOffset].y, bitToSet); break;\n"
			"    case 2: atomicOr(superSecretComputeShaderHelper[groupOffset].z, bitToSet); break;\n"
			"    case 3: atomicOr(superSecretComputeShaderHelper[groupOffset].w, bitToSet); break;\n"
			"    }\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  return packUint2x32(superSecretComputeShaderHelper[groupOffset].xy);\n"
			"}\n";
}

deUint32 vkt::subgroups::getSubgroupSize (Context& context)
{
	return context.getSubgroupProperties().subgroupSize;
}

deUint32 vkt::subgroups::maxSupportedSubgroupSize ()
{
	return 128u;
}

std::string vkt::subgroups::getShaderStageName (VkShaderStageFlags stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_COMPUTE_BIT:					return "compute";
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return "fragment";
		case VK_SHADER_STAGE_VERTEX_BIT:					return "vertex";
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return "geometry";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return "tess_control";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return "tess_eval";
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:				return "rgen";
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:				return "ahit";
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:			return "chit";
		case VK_SHADER_STAGE_MISS_BIT_KHR:					return "miss";
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:			return "sect";
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:				return "call";
		default:											TCU_THROW(InternalError, "Unhandled stage");
	}
}

std::string vkt::subgroups::getSubgroupFeatureName (vk::VkSubgroupFeatureFlagBits bit)
{
	switch (bit)
	{
		case VK_SUBGROUP_FEATURE_BASIC_BIT:				return "VK_SUBGROUP_FEATURE_BASIC_BIT";
		case VK_SUBGROUP_FEATURE_VOTE_BIT:				return "VK_SUBGROUP_FEATURE_VOTE_BIT";
		case VK_SUBGROUP_FEATURE_ARITHMETIC_BIT:		return "VK_SUBGROUP_FEATURE_ARITHMETIC_BIT";
		case VK_SUBGROUP_FEATURE_BALLOT_BIT:			return "VK_SUBGROUP_FEATURE_BALLOT_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_BIT:			return "VK_SUBGROUP_FEATURE_SHUFFLE_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT:	return "VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT";
		case VK_SUBGROUP_FEATURE_CLUSTERED_BIT:			return "VK_SUBGROUP_FEATURE_CLUSTERED_BIT";
		case VK_SUBGROUP_FEATURE_QUAD_BIT:				return "VK_SUBGROUP_FEATURE_QUAD_BIT";
		default:										TCU_THROW(InternalError, "Unknown subgroup feature category");
	}
}

void vkt::subgroups::addNoSubgroupShader (SourceCollections& programCollection)
{
	{
	/*
		"#version 450\n"
		"void main (void)\n"
		"{\n"
		"  float pixelSize = 2.0f/1024.0f;\n"
		"   float pixelPosition = pixelSize/2.0f - 1.0f;\n"
		"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
		"  gl_PointSize = 1.0f;\n"
		"}\n"
	*/
		const std::string vertNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 37\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %22 %26\n"
			"OpMemberDecorate %20 0 BuiltIn Position\n"
			"OpMemberDecorate %20 1 BuiltIn PointSize\n"
			"OpMemberDecorate %20 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %20 3 BuiltIn CullDistance\n"
			"OpDecorate %20 Block\n"
			"OpDecorate %26 BuiltIn VertexIndex\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0.00195313\n"
			"%12 = OpConstant %6 2\n"
			"%14 = OpConstant %6 1\n"
			"%16 = OpTypeVector %6 4\n"
			"%17 = OpTypeInt 32 0\n"
			"%18 = OpConstant %17 1\n"
			"%19 = OpTypeArray %6 %18\n"
			"%20 = OpTypeStruct %16 %6 %19 %19\n"
			"%21 = OpTypePointer Output %20\n"
			"%22 = OpVariable %21 Output\n"
			"%23 = OpTypeInt 32 1\n"
			"%24 = OpConstant %23 0\n"
			"%25 = OpTypePointer Input %23\n"
			"%26 = OpVariable %25 Input\n"
			"%33 = OpConstant %6 0\n"
			"%35 = OpTypePointer Output %16\n"
			"%37 = OpConstant %23 1\n"
			"%38 = OpTypePointer Output %6\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%10 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%11 = OpLoad %6 %8\n"
			"%13 = OpFDiv %6 %11 %12\n"
			"%15 = OpFSub %6 %13 %14\n"
			"OpStore %10 %15\n"
			"%27 = OpLoad %23 %26\n"
			"%28 = OpConvertSToF %6 %27\n"
			"%29 = OpLoad %6 %8\n"
			"%30 = OpFMul %6 %28 %29\n"
			"%31 = OpLoad %6 %10\n"
			"%32 = OpFAdd %6 %30 %31\n"
			"%34 = OpCompositeConstruct %16 %32 %33 %33 %14\n"
			"%36 = OpAccessChain %35 %22 %24\n"
			"OpStore %36 %34\n"
			"%39 = OpAccessChain %38 %22 %37\n"
			"OpStore %39 %14\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert_noSubgroup") << vertNoSubgroup;
	}

	{
	/*
		"#version 450\n"
		"layout(vertices=1) out;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  if (gl_InvocationID == 0)\n"
		"  {\n"
		"    gl_TessLevelOuter[0] = 1.0f;\n"
		"    gl_TessLevelOuter[1] = 1.0f;\n"
		"  }\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n"
	*/
		const std::string tescNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 45\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %32 %38\n"
			"OpExecutionMode %4 OutputVertices 1\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			"OpMemberDecorate %29 0 BuiltIn Position\n"
			"OpMemberDecorate %29 1 BuiltIn PointSize\n"
			"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
			"OpDecorate %29 Block\n"
			"OpMemberDecorate %34 0 BuiltIn Position\n"
			"OpMemberDecorate %34 1 BuiltIn PointSize\n"
			"OpMemberDecorate %34 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %34 3 BuiltIn CullDistance\n"
			"OpDecorate %34 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 1\n"
			"%7 = OpTypePointer Input %6\n"
			"%8 = OpVariable %7 Input\n"
			"%10 = OpConstant %6 0\n"
			"%11 = OpTypeBool\n"
			"%15 = OpTypeFloat 32\n"
			"%16 = OpTypeInt 32 0\n"
			"%17 = OpConstant %16 4\n"
			"%18 = OpTypeArray %15 %17\n"
			"%19 = OpTypePointer Output %18\n"
			"%20 = OpVariable %19 Output\n"
			"%21 = OpConstant %15 1\n"
			"%22 = OpTypePointer Output %15\n"
			"%24 = OpConstant %6 1\n"
			"%26 = OpTypeVector %15 4\n"
			"%27 = OpConstant %16 1\n"
			"%28 = OpTypeArray %15 %27\n"
			"%29 = OpTypeStruct %26 %15 %28 %28\n"
			"%30 = OpTypeArray %29 %27\n"
			"%31 = OpTypePointer Output %30\n"
			"%32 = OpVariable %31 Output\n"
			"%34 = OpTypeStruct %26 %15 %28 %28\n"
			"%35 = OpConstant %16 32\n"
			"%36 = OpTypeArray %34 %35\n"
			"%37 = OpTypePointer Input %36\n"
			"%38 = OpVariable %37 Input\n"
			"%40 = OpTypePointer Input %26\n"
			"%43 = OpTypePointer Output %26\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%9 = OpLoad %6 %8\n"
			"%12 = OpIEqual %11 %9 %10\n"
			"OpSelectionMerge %14 None\n"
			"OpBranchConditional %12 %13 %14\n"
			"%13 = OpLabel\n"
			"%23 = OpAccessChain %22 %20 %10\n"
			"OpStore %23 %21\n"
			"%25 = OpAccessChain %22 %20 %24\n"
			"OpStore %25 %21\n"
			"OpBranch %14\n"
			"%14 = OpLabel\n"
			"%33 = OpLoad %6 %8\n"
			"%39 = OpLoad %6 %8\n"
			"%41 = OpAccessChain %40 %38 %39 %10\n"
			"%42 = OpLoad %26 %41\n"
			"%44 = OpAccessChain %43 %32 %33 %10\n"
			"OpStore %44 %42\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc_noSubgroup") << tescNoSubgroup;
	}

	{
	/*
		"#version 450\n"
		"layout(isolines) in;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  float pixelSize = 2.0f/1024.0f;\n"
		"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
		"}\n";
	*/
		const std::string teseNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 42\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %16 %23 %29\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			"OpMemberDecorate %14 0 BuiltIn Position\n"
			"OpMemberDecorate %14 1 BuiltIn PointSize\n"
			"OpMemberDecorate %14 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %14 3 BuiltIn CullDistance\n"
			"OpDecorate %14 Block\n"
			"OpMemberDecorate %19 0 BuiltIn Position\n"
			"OpMemberDecorate %19 1 BuiltIn PointSize\n"
			"OpMemberDecorate %19 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %19 3 BuiltIn CullDistance\n"
			"OpDecorate %19 Block\n"
			"OpDecorate %29 BuiltIn TessCoord\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0.00195313\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpTypeInt 32 0\n"
			"%12 = OpConstant %11 1\n"
			"%13 = OpTypeArray %6 %12\n"
			"%14 = OpTypeStruct %10 %6 %13 %13\n"
			"%15 = OpTypePointer Output %14\n"
			"%16 = OpVariable %15 Output\n"
			"%17 = OpTypeInt 32 1\n"
			"%18 = OpConstant %17 0\n"
			"%19 = OpTypeStruct %10 %6 %13 %13\n"
			"%20 = OpConstant %11 32\n"
			"%21 = OpTypeArray %19 %20\n"
			"%22 = OpTypePointer Input %21\n"
			"%23 = OpVariable %22 Input\n"
			"%24 = OpTypePointer Input %10\n"
			"%27 = OpTypeVector %6 3\n"
			"%28 = OpTypePointer Input %27\n"
			"%29 = OpVariable %28 Input\n"
			"%30 = OpConstant %11 0\n"
			"%31 = OpTypePointer Input %6\n"
			"%36 = OpConstant %6 2\n"
			"%40 = OpTypePointer Output %10\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%25 = OpAccessChain %24 %23 %18 %18\n"
			"%26 = OpLoad %10 %25\n"
			"%32 = OpAccessChain %31 %29 %30\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpLoad %6 %8\n"
			"%35 = OpFMul %6 %33 %34\n"
			"%37 = OpFDiv %6 %35 %36\n"
			"%38 = OpCompositeConstruct %10 %37 %37 %37 %37\n"
			"%39 = OpFAdd %10 %26 %38\n"
			"%41 = OpAccessChain %40 %16 %18\n"
			"OpStore %41 %39\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese_noSubgroup") << teseNoSubgroup;
	}

}

static std::string getFramebufferBufferDeclarations (const VkFormat&					format,
													 const std::vector<std::string>&	declarations,
													 const deUint32						stage)
{
	if (declarations.empty())
	{
		const std::string	name	= (stage == 0) ? "result" : "out_color";
		const std::string	suffix	= (stage == 2) ? "[]" : "";
		const std::string	result	=
			"layout(location = 0) out float " + name + suffix + ";\n"
			"layout(set = 0, binding = 0) uniform Buffer1\n"
			"{\n"
			"  " + de::toString(subgroups::getFormatNameForGLSL(format)) + " data[" + de::toString(subgroups::maxSupportedSubgroupSize()) + "];\n"
			"};\n";

		return result;
	}
	else
	{
		return declarations[stage];
	}
}

void vkt::subgroups::initStdFrameBufferPrograms (SourceCollections&					programCollection,
												 const vk::ShaderBuildOptions&		buildOptions,
												 VkShaderStageFlags					shaderStage,
												 VkFormat							format,
												 bool								gsPointSize,
												 const std::string&					extHeader,
												 const std::string&					testSrc,
												 const std::string&					helperStr,
												 const std::vector<std::string>&	declarations)
{
	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (shaderStage != VK_SHADER_STAGE_VERTEX_BIT)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
	{
		std::ostringstream vertex;

		vertex << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extHeader
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< getFramebufferBufferDeclarations(format, declarations, 0)
			<< "\n"
			<< helperStr
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempRes;\n"
			<< testSrc
			<< "  result = float(tempRes);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vertex.str()) << buildOptions;
	}
	else if (shaderStage == VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extHeader
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< getFramebufferBufferDeclarations(format, declarations, 1)
			<< "\n"
			<< helperStr
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempRes;\n"
			<< testSrc
			<< "  out_color = float(tempRes);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< (gsPointSize ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.glslSources.add("geometry") << glu::GeometrySource(geometry.str()) << buildOptions;
	}
	else if (shaderStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		std::ostringstream controlSource;

		controlSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extHeader
			<< "layout(vertices = 2) out;\n"
			<< getFramebufferBufferDeclarations(format, declarations, 2)
			<< "\n"
			<< helperStr
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<< "  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< "  uint tempRes;\n"
			<< testSrc
			<< "  out_color[gl_InvocationID] = float(tempRes);\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< (gsPointSize ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "")
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (shaderStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		ostringstream evaluationSource;

		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extHeader
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< getFramebufferBufferDeclarations(format, declarations, 3)
			<< "\n"
			<< helperStr
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint tempRes;\n"
			<< testSrc
			<< "  out_color = float(tempRes);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< (gsPointSize ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

static std::string getBufferDeclarations (vk::VkShaderStageFlags			shaderStage,
										  const std::string&				formatName,
										  const std::vector<std::string>&	declarations,
										  const deUint32					stage)
{
	if (declarations.empty())
	{
		const deUint32	stageCount	= vkt::subgroups::getStagesCount(shaderStage);
		const deUint32	binding0	= stage;
		const deUint32	binding1	= stageCount;
		const bool		fragment	= (shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) && (stage == stageCount);
		const string	buffer1		= fragment
									? "layout(location = 0) out uint result;\n"
									: "layout(set = 0, binding = " + de::toString(binding0) + ", std430) buffer Buffer1\n"
									  "{\n"
									  "  uint result[];\n"
									  "};\n";
		//todo boza I suppose it can be "layout(set = 0, binding = " + de::toString(binding1) + ", std430) readonly buffer Buffer2\n"
		const string	buffer2		= "layout(set = 0, binding = " + de::toString(binding1) + ", std430)" + (stageCount == 1 ? "" : " readonly") + " buffer Buffer" + (fragment ? "1" : "2") + "\n"
									  "{\n"
									  "  " + formatName + " data[];\n"
									  "};\n";

		return buffer1 + buffer2;
	}
	else
	{
		return declarations[stage];
	}
}

void vkt::subgroups::initStdPrograms (vk::SourceCollections&			programCollection,
									  const vk::ShaderBuildOptions&		buildOptions,
									  vk::VkShaderStageFlags			shaderStage,
									  vk::VkFormat						format,
									  bool								gsPointSize,
									  const std::string&				extHeader,
									  const std::string&				testSrc,
									  const std::string&				helperStr,
									  const std::vector<std::string>&	declarations,
									  const bool						avoidHelperInvocations,
									  const std::string&				tempRes)
{
	const std::string	formatName	= subgroups::getFormatNameForGLSL(format);

	if (isAllComputeStages(shaderStage))
	{
		std::ostringstream	src;

		src << "#version 450\n"
			<< extHeader
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< getBufferDeclarations(shaderStage, formatName, declarations, 0)
			<< "\n"
			<< helperStr
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< tempRes
			<< testSrc
			<< "  result[offset] = tempRes;\n"
			<< "}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(src.str()) << buildOptions;
	}
	else if (isAllGraphicsStages(shaderStage))
	{
		const string vertex =
			"#version 450\n"
			+ extHeader
			+ getBufferDeclarations(shaderStage, formatName, declarations, 0) +
			"\n"
			+ helperStr +
			"void main (void)\n"
			"{\n"
			"  uint tempRes;\n"
			+ testSrc +
			"  result[gl_VertexIndex] = tempRes;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			+ extHeader +
			"layout(vertices=1) out;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 1) +
			"\n"
			+ helperStr +
			"void main (void)\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_PrimitiveID] = tempRes;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			+ (gsPointSize ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "") +
			"}\n";

		const string tese =
			"#version 450\n"
			+ extHeader +
			"layout(isolines) in;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 2) +
			"\n"
			+ helperStr +
			"void main (void)\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempRes;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			+ (gsPointSize ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
			"}\n";

		const string geometry =
			"#version 450\n"
			+ extHeader +
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 3) +
			"\n"
			+ helperStr +
			"void main (void)\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_PrimitiveIDIn] = tempRes;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			+ (gsPointSize ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			+ extHeader
			+ getBufferDeclarations(shaderStage, formatName, declarations, 4)
			+ helperStr +
			"void main (void)\n"
			"{\n"
			+ (avoidHelperInvocations ? "  if (gl_HelperInvocation) return;\n" : "")
			+ tempRes
			+ testSrc +
			"  result = tempRes;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert") << glu::VertexSource(vertex) << buildOptions;
		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc) << buildOptions;
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese) << buildOptions;
		subgroups::addGeometryShadersFromTemplate(geometry, buildOptions, programCollection.glslSources);
		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment)<< buildOptions;
	}
	else if (isAllRayTracingStages(shaderStage))
	{
		const std::string	rgenShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"layout(location = 0) rayPayloadEXT uvec4 payload;\n"
			"layout(location = 0) callableDataEXT uvec4 callData;"
			"layout(set = 1, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 0) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  uint  rayFlags   = 0;\n"
			"  uint  cullMask   = 0xFF;\n"
			"  float tmin       = 0.0;\n"
			"  float tmax       = 9.0;\n"
			"  vec3  origin     = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
			"  vec3  directHit  = vec3(0.0, 0.0, -1.0);\n"
			"  vec3  directMiss = vec3(0.0, 0.0, +1.0);\n"
			"\n"
			"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, directHit, tmax, 0);\n"
			"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, directMiss, tmax, 0);\n"
			"  executeCallableEXT(0, 0);"
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";
		const std::string	ahitShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"hitAttributeEXT vec3 attribs;\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 1) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";
		const std::string	chitShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"hitAttributeEXT vec3 attribs;\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 2) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";
		const std::string	missShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 3) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";
		const std::string	sectShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"hitAttributeEXT vec3 hitAttribute;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 4) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  reportIntersectionEXT(0.75f, 0x7Eu);\n"
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";
		const std::string	callShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing: require\n"
			+ extHeader +
			"layout(location = 0) callableDataInEXT float callData;\n"
			+ getBufferDeclarations(shaderStage, formatName, declarations, 5) +
			"\n"
			+ helperStr +
			"void main()\n"
			"{\n"
			+ tempRes
			+ testSrc +
			"  result[gl_LaunchIDEXT.x] = tempRes;\n"
			"}\n";

		programCollection.glslSources.add("rgen") << glu::RaygenSource		(rgenShader) << buildOptions;
		programCollection.glslSources.add("ahit") << glu::AnyHitSource		(ahitShader) << buildOptions;
		programCollection.glslSources.add("chit") << glu::ClosestHitSource	(chitShader) << buildOptions;
		programCollection.glslSources.add("miss") << glu::MissSource		(missShader) << buildOptions;
		programCollection.glslSources.add("sect") << glu::IntersectionSource(sectShader) << buildOptions;
		programCollection.glslSources.add("call") << glu::CallableSource	(callShader) << buildOptions;

		subgroups::addRayTracingNoSubgroupShader(programCollection);
	}
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");

}

bool vkt::subgroups::isSubgroupSupported (Context& context)
{
	return context.contextSupports(vk::ApiVersion(1, 1, 0));
}

bool vkt::subgroups::areSubgroupOperationsSupportedForStage (Context& context, const VkShaderStageFlags stage)
{
	return (stage & (context.getSubgroupProperties().supportedStages)) ? true : false;
}

bool vkt::subgroups::isSubgroupFeatureSupportedForDevice (Context& context, VkSubgroupFeatureFlagBits bit)
{
	return (bit & (context.getSubgroupProperties().supportedOperations)) ? true : false;
}

bool vkt::subgroups::isFragmentSSBOSupportedForDevice (Context& context)
{
	return context.getDeviceFeatures().fragmentStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isVertexSSBOSupportedForDevice (Context& context)
{
	return context.getDeviceFeatures().vertexPipelineStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isInt64SupportedForDevice (Context& context)
{
	return context.getDeviceFeatures().shaderInt64 ? true : false;
}

bool vkt::subgroups::isTessellationAndGeometryPointSizeSupported (Context& context)
{
	return context.getDeviceFeatures().shaderTessellationAndGeometryPointSize ? true : false;
}

bool vkt::subgroups::is16BitUBOStorageSupported (Context& context)
{
	return context.get16BitStorageFeatures().uniformAndStorageBuffer16BitAccess ? true : false;
}

bool vkt::subgroups::is8BitUBOStorageSupported (Context& context)
{
	return context.get8BitStorageFeatures().uniformAndStorageBuffer8BitAccess ? true : false;
}

bool vkt::subgroups::isFormatSupportedForDevice (Context& context, vk::VkFormat format)
{
	const VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures&	subgroupExtendedTypesFeatures	= context.getShaderSubgroupExtendedTypesFeatures();
	const VkPhysicalDeviceShaderFloat16Int8Features&			float16Int8Features				= context.getShaderFloat16Int8Features();
	const VkPhysicalDevice16BitStorageFeatures&					storage16bit					= context.get16BitStorageFeatures();
	const VkPhysicalDevice8BitStorageFeatures&					storage8bit						= context.get8BitStorageFeatures();
	const VkPhysicalDeviceFeatures&								features						= context.getDeviceFeatures();
	bool														shaderFloat64					= features.shaderFloat64 ? true : false;
	bool														shaderInt16						= features.shaderInt16 ? true : false;
	bool														shaderInt64						= features.shaderInt64 ? true : false;
	bool														shaderSubgroupExtendedTypes		= false;
	bool														shaderFloat16					= false;
	bool														shaderInt8						= false;
	bool														storageBuffer16BitAccess		= false;
	bool														storageBuffer8BitAccess			= false;

	if (context.isDeviceFunctionalitySupported("VK_KHR_shader_subgroup_extended_types") &&
		context.isDeviceFunctionalitySupported("VK_KHR_shader_float16_int8"))
	{
		shaderSubgroupExtendedTypes	= subgroupExtendedTypesFeatures.shaderSubgroupExtendedTypes ? true : false;
		shaderFloat16				= float16Int8Features.shaderFloat16 ? true : false;
		shaderInt8					= float16Int8Features.shaderInt8 ? true : false;

		if ( context.isDeviceFunctionalitySupported("VK_KHR_16bit_storage") )
			storageBuffer16BitAccess = storage16bit.storageBuffer16BitAccess ? true : false;

		if (context.isDeviceFunctionalitySupported("VK_KHR_8bit_storage"))
			storageBuffer8BitAccess = storage8bit.storageBuffer8BitAccess ? true : false;
	}

	switch (format)
	{
		default:
			return true;
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return shaderSubgroupExtendedTypes && shaderFloat16 && storageBuffer16BitAccess;
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return shaderFloat64;
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8A8_UINT:
			return shaderSubgroupExtendedTypes && shaderInt8 && storageBuffer8BitAccess;
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16A16_UINT:
			return shaderSubgroupExtendedTypes && shaderInt16 && storageBuffer16BitAccess;
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64A64_UINT:
			return shaderSubgroupExtendedTypes && shaderInt64;
	}
}

bool vkt::subgroups::isSubgroupBroadcastDynamicIdSupported (Context& context)
{
	return context.contextSupports(vk::ApiVersion(1, 2, 0)) && context.getDeviceVulkan12Features().subgroupBroadcastDynamicId;
}

std::string vkt::subgroups::getFormatNameForGLSL (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R8_SINT:				return "int8_t";
		case VK_FORMAT_R8G8_SINT:			return "i8vec2";
		case VK_FORMAT_R8G8B8_SINT:			return "i8vec3";
		case VK_FORMAT_R8G8B8A8_SINT:		return "i8vec4";
		case VK_FORMAT_R8_UINT:				return "uint8_t";
		case VK_FORMAT_R8G8_UINT:			return "u8vec2";
		case VK_FORMAT_R8G8B8_UINT:			return "u8vec3";
		case VK_FORMAT_R8G8B8A8_UINT:		return "u8vec4";
		case VK_FORMAT_R16_SINT:			return "int16_t";
		case VK_FORMAT_R16G16_SINT:			return "i16vec2";
		case VK_FORMAT_R16G16B16_SINT:		return "i16vec3";
		case VK_FORMAT_R16G16B16A16_SINT:	return "i16vec4";
		case VK_FORMAT_R16_UINT:			return "uint16_t";
		case VK_FORMAT_R16G16_UINT:			return "u16vec2";
		case VK_FORMAT_R16G16B16_UINT:		return "u16vec3";
		case VK_FORMAT_R16G16B16A16_UINT:	return "u16vec4";
		case VK_FORMAT_R32_SINT:			return "int";
		case VK_FORMAT_R32G32_SINT:			return "ivec2";
		case VK_FORMAT_R32G32B32_SINT:		return "ivec3";
		case VK_FORMAT_R32G32B32A32_SINT:	return "ivec4";
		case VK_FORMAT_R32_UINT:			return "uint";
		case VK_FORMAT_R32G32_UINT:			return "uvec2";
		case VK_FORMAT_R32G32B32_UINT:		return "uvec3";
		case VK_FORMAT_R32G32B32A32_UINT:	return "uvec4";
		case VK_FORMAT_R64_SINT:			return "int64_t";
		case VK_FORMAT_R64G64_SINT:			return "i64vec2";
		case VK_FORMAT_R64G64B64_SINT:		return "i64vec3";
		case VK_FORMAT_R64G64B64A64_SINT:	return "i64vec4";
		case VK_FORMAT_R64_UINT:			return "uint64_t";
		case VK_FORMAT_R64G64_UINT:			return "u64vec2";
		case VK_FORMAT_R64G64B64_UINT:		return "u64vec3";
		case VK_FORMAT_R64G64B64A64_UINT:	return "u64vec4";
		case VK_FORMAT_R16_SFLOAT:			return "float16_t";
		case VK_FORMAT_R16G16_SFLOAT:		return "f16vec2";
		case VK_FORMAT_R16G16B16_SFLOAT:	return "f16vec3";
		case VK_FORMAT_R16G16B16A16_SFLOAT:	return "f16vec4";
		case VK_FORMAT_R32_SFLOAT:			return "float";
		case VK_FORMAT_R32G32_SFLOAT:		return "vec2";
		case VK_FORMAT_R32G32B32_SFLOAT:	return "vec3";
		case VK_FORMAT_R32G32B32A32_SFLOAT:	return "vec4";
		case VK_FORMAT_R64_SFLOAT:			return "double";
		case VK_FORMAT_R64G64_SFLOAT:		return "dvec2";
		case VK_FORMAT_R64G64B64_SFLOAT:	return "dvec3";
		case VK_FORMAT_R64G64B64A64_SFLOAT:	return "dvec4";
		case VK_FORMAT_R8_USCALED:			return "bool";
		case VK_FORMAT_R8G8_USCALED:		return "bvec2";
		case VK_FORMAT_R8G8B8_USCALED:		return "bvec3";
		case VK_FORMAT_R8G8B8A8_USCALED:	return "bvec4";
		default:							TCU_THROW(InternalError, "Unhandled format");
	}
}

std::string vkt::subgroups::getAdditionalExtensionForFormat (vk::VkFormat format)
{
	switch (format)
	{
		default:
			return "";
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8A8_UINT:
			return "#extension GL_EXT_shader_subgroup_extended_types_int8 : enable\n";
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16A16_UINT:
			return "#extension GL_EXT_shader_subgroup_extended_types_int16 : enable\n";
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64A64_UINT:
			return "#extension GL_EXT_shader_subgroup_extended_types_int64 : enable\n";
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return "#extension GL_EXT_shader_subgroup_extended_types_float16 : enable\n";
	}
}

const std::vector<vk::VkFormat> vkt::subgroups::getAllFormats ()
{
	std::vector<VkFormat> formats;

	formats.push_back(VK_FORMAT_R8_SINT);
	formats.push_back(VK_FORMAT_R8G8_SINT);
	formats.push_back(VK_FORMAT_R8G8B8_SINT);
	formats.push_back(VK_FORMAT_R8G8B8A8_SINT);
	formats.push_back(VK_FORMAT_R8_UINT);
	formats.push_back(VK_FORMAT_R8G8_UINT);
	formats.push_back(VK_FORMAT_R8G8B8_UINT);
	formats.push_back(VK_FORMAT_R8G8B8A8_UINT);
	formats.push_back(VK_FORMAT_R16_SINT);
	formats.push_back(VK_FORMAT_R16G16_SINT);
	formats.push_back(VK_FORMAT_R16G16B16_SINT);
	formats.push_back(VK_FORMAT_R16G16B16A16_SINT);
	formats.push_back(VK_FORMAT_R16_UINT);
	formats.push_back(VK_FORMAT_R16G16_UINT);
	formats.push_back(VK_FORMAT_R16G16B16_UINT);
	formats.push_back(VK_FORMAT_R16G16B16A16_UINT);
	formats.push_back(VK_FORMAT_R32_SINT);
	formats.push_back(VK_FORMAT_R32G32_SINT);
	formats.push_back(VK_FORMAT_R32G32B32_SINT);
	formats.push_back(VK_FORMAT_R32G32B32A32_SINT);
	formats.push_back(VK_FORMAT_R32_UINT);
	formats.push_back(VK_FORMAT_R32G32_UINT);
	formats.push_back(VK_FORMAT_R32G32B32_UINT);
	formats.push_back(VK_FORMAT_R32G32B32A32_UINT);
	formats.push_back(VK_FORMAT_R64_SINT);
	formats.push_back(VK_FORMAT_R64G64_SINT);
	formats.push_back(VK_FORMAT_R64G64B64_SINT);
	formats.push_back(VK_FORMAT_R64G64B64A64_SINT);
	formats.push_back(VK_FORMAT_R64_UINT);
	formats.push_back(VK_FORMAT_R64G64_UINT);
	formats.push_back(VK_FORMAT_R64G64B64_UINT);
	formats.push_back(VK_FORMAT_R64G64B64A64_UINT);
	formats.push_back(VK_FORMAT_R16_SFLOAT);
	formats.push_back(VK_FORMAT_R16G16_SFLOAT);
	formats.push_back(VK_FORMAT_R16G16B16_SFLOAT);
	formats.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);
	formats.push_back(VK_FORMAT_R32_SFLOAT);
	formats.push_back(VK_FORMAT_R32G32_SFLOAT);
	formats.push_back(VK_FORMAT_R32G32B32_SFLOAT);
	formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
	formats.push_back(VK_FORMAT_R64_SFLOAT);
	formats.push_back(VK_FORMAT_R64G64_SFLOAT);
	formats.push_back(VK_FORMAT_R64G64B64_SFLOAT);
	formats.push_back(VK_FORMAT_R64G64B64A64_SFLOAT);
	formats.push_back(VK_FORMAT_R8_USCALED);
	formats.push_back(VK_FORMAT_R8G8_USCALED);
	formats.push_back(VK_FORMAT_R8G8B8_USCALED);
	formats.push_back(VK_FORMAT_R8G8B8A8_USCALED);

	return formats;
}

bool vkt::subgroups::isFormatSigned (VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64A64_SINT:
			return true;
	}
}

bool vkt::subgroups::isFormatUnsigned (VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64A64_UINT:
			return true;
	}
}

bool vkt::subgroups::isFormatFloat (VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return true;
	}
}

bool vkt::subgroups::isFormatBool (VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
			return true;
	}
}

bool vkt::subgroups::isFormat8bitTy (VkFormat format)
{
	switch (format)
	{
	default:
		return false;
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8B8_SINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R8G8B8_UINT:
	case VK_FORMAT_R8G8B8A8_UINT:
		return true;
	}
}

bool vkt::subgroups::isFormat16BitTy (VkFormat format)
{
	switch (format)
	{
	default:
		return false;
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16B16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16B16_SINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R16G16B16_UINT:
	case VK_FORMAT_R16G16B16A16_UINT:
		return true;
	}
}

void vkt::subgroups::setVertexShaderFrameBuffer (SourceCollections& programCollection)
{
	/*
		"layout(location = 0) in highp vec4 in_position;\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = in_position;\n"
		"  gl_PointSize = 1.0f;\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("vert") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 7\n"
		"; Bound: 25\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %13 %17\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpDecorate %17 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypePointer Input %7\n"
		"%17 = OpVariable %16 Input\n"
		"%19 = OpTypePointer Output %7\n"
		"%21 = OpConstant %14 1\n"
		"%22 = OpConstant %6 1\n"
		"%23 = OpTypePointer Output %6\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpLoad %7 %17\n"
		"%20 = OpAccessChain %19 %13 %15\n"
		"OpStore %20 %18\n"
		"%24 = OpAccessChain %23 %13 %21\n"
		"OpStore %24 %22\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"layout(location = 0) in float in_color;\n"
		"layout(location = 0) out uint out_color;\n"
		"void main()\n"
		{\n"
		"	out_color = uint(in_color);\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("fragment") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 14\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %8 %11\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpDecorate %8 Location 0\n"
		"OpDecorate %11 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 0\n"
		"%7 = OpTypePointer Output %6\n"
		"%8 = OpVariable %7 Output\n"
		"%9 = OpTypeFloat 32\n"
		"%10 = OpTypePointer Input %9\n"
		"%11 = OpVariable %10 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%12 = OpLoad %9 %11\n"
		"%13 = OpConvertFToU %6 %12\n"
		"OpStore %8 %13\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setTesCtrlShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"#extension GL_KHR_shader_subgroup_basic: enable\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(vertices = 2) out;\n"
		"void main (void)\n"
		"{\n"
		"  if (gl_InvocationID == 0)\n"
		"  {\n"
		"    gl_TessLevelOuter[0] = 1.0f;\n"
		"    gl_TessLevelOuter[1] = 1.0f;\n"
		"  }\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("tesc") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 46\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %33 %39\n"
		"OpExecutionMode %4 OutputVertices 2\n"
		"OpDecorate %8 BuiltIn InvocationId\n"
		"OpDecorate %20 Patch\n"
		"OpDecorate %20 BuiltIn TessLevelOuter\n"
		"OpMemberDecorate %29 0 BuiltIn Position\n"
		"OpMemberDecorate %29 1 BuiltIn PointSize\n"
		"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
		"OpDecorate %29 Block\n"
		"OpMemberDecorate %35 0 BuiltIn Position\n"
		"OpMemberDecorate %35 1 BuiltIn PointSize\n"
		"OpMemberDecorate %35 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %35 3 BuiltIn CullDistance\n"
		"OpDecorate %35 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 1\n"
		"%7 = OpTypePointer Input %6\n"
		"%8 = OpVariable %7 Input\n"
		"%10 = OpConstant %6 0\n"
		"%11 = OpTypeBool\n"
		"%15 = OpTypeFloat 32\n"
		"%16 = OpTypeInt 32 0\n"
		"%17 = OpConstant %16 4\n"
		"%18 = OpTypeArray %15 %17\n"
		"%19 = OpTypePointer Output %18\n"
		"%20 = OpVariable %19 Output\n"
		"%21 = OpConstant %15 1\n"
		"%22 = OpTypePointer Output %15\n"
		"%24 = OpConstant %6 1\n"
		"%26 = OpTypeVector %15 4\n"
		"%27 = OpConstant %16 1\n"
		"%28 = OpTypeArray %15 %27\n"
		"%29 = OpTypeStruct %26 %15 %28 %28\n"
		"%30 = OpConstant %16 2\n"
		"%31 = OpTypeArray %29 %30\n"
		"%32 = OpTypePointer Output %31\n"
		"%33 = OpVariable %32 Output\n"
		"%35 = OpTypeStruct %26 %15 %28 %28\n"
		"%36 = OpConstant %16 32\n"
		"%37 = OpTypeArray %35 %36\n"
		"%38 = OpTypePointer Input %37\n"
		"%39 = OpVariable %38 Input\n"
		"%41 = OpTypePointer Input %26\n"
		"%44 = OpTypePointer Output %26\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%9 = OpLoad %6 %8\n"
		"%12 = OpIEqual %11 %9 %10\n"
		"OpSelectionMerge %14 None\n"
		"OpBranchConditional %12 %13 %14\n"
		"%13 = OpLabel\n"
		"%23 = OpAccessChain %22 %20 %10\n"
		"OpStore %23 %21\n"
		"%25 = OpAccessChain %22 %20 %24\n"
		"OpStore %25 %21\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%34 = OpLoad %6 %8\n"
		"%40 = OpLoad %6 %8\n"
		"%42 = OpAccessChain %41 %39 %40 %10\n"
		"%43 = OpLoad %26 %42\n"
		"%45 = OpAccessChain %44 %33 %34 %10\n"
		"OpStore %45 %43\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setTesEvalShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"#extension GL_KHR_shader_subgroup_ballot: enable\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(isolines, equal_spacing, ccw ) in;\n"
		"layout(location = 0) in float in_color[];\n"
		"layout(location = 0) out float out_color;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
		"  out_color = in_color[0];\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("tese") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 45\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %4 \"main\" %13 %20 %29 %39 %42\n"
		"OpExecutionMode %4 Isolines\n"
		"OpExecutionMode %4 SpacingEqual\n"
		"OpExecutionMode %4 VertexOrderCcw\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpMemberDecorate %16 0 BuiltIn Position\n"
		"OpMemberDecorate %16 1 BuiltIn PointSize\n"
		"OpMemberDecorate %16 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %16 3 BuiltIn CullDistance\n"
		"OpDecorate %16 Block\n"
		"OpDecorate %29 BuiltIn TessCoord\n"
		"OpDecorate %39 Location 0\n"
		"OpDecorate %42 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypeStruct %7 %6 %10 %10\n"
		"%17 = OpConstant %8 32\n"
		"%18 = OpTypeArray %16 %17\n"
		"%19 = OpTypePointer Input %18\n"
		"%20 = OpVariable %19 Input\n"
		"%21 = OpTypePointer Input %7\n"
		"%24 = OpConstant %14 1\n"
		"%27 = OpTypeVector %6 3\n"
		"%28 = OpTypePointer Input %27\n"
		"%29 = OpVariable %28 Input\n"
		"%30 = OpConstant %8 0\n"
		"%31 = OpTypePointer Input %6\n"
		"%36 = OpTypePointer Output %7\n"
		"%38 = OpTypePointer Output %6\n"
		"%39 = OpVariable %38 Output\n"
		"%40 = OpTypeArray %6 %17\n"
		"%41 = OpTypePointer Input %40\n"
		"%42 = OpVariable %41 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%22 = OpAccessChain %21 %20 %15 %15\n"
		"%23 = OpLoad %7 %22\n"
		"%25 = OpAccessChain %21 %20 %24 %15\n"
		"%26 = OpLoad %7 %25\n"
		"%32 = OpAccessChain %31 %29 %30\n"
		"%33 = OpLoad %6 %32\n"
		"%34 = OpCompositeConstruct %7 %33 %33 %33 %33\n"
		"%35 = OpExtInst %7 %1 FMix %23 %26 %34\n"
		"%37 = OpAccessChain %36 %13 %15\n"
		"OpStore %37 %35\n"
		"%43 = OpAccessChain %31 %42 %15\n"
		"%44 = OpLoad %6 %43\n"
		"OpStore %39 %44\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::addGeometryShadersFromTemplate (const std::string& glslTemplate, const vk::ShaderBuildOptions& options,  vk::GlslSourceCollection& collection)
{
	tcu::StringTemplate geometryTemplate(glslTemplate);

	map<string, string>		linesParams;
	linesParams.insert(pair<string, string>("TOPOLOGY", "lines"));

	map<string, string>		pointsParams;
	pointsParams.insert(pair<string, string>("TOPOLOGY", "points"));

	collection.add("geometry_lines")	<< glu::GeometrySource(geometryTemplate.specialize(linesParams))	<< options;
	collection.add("geometry_points")	<< glu::GeometrySource(geometryTemplate.specialize(pointsParams))	<< options;
}

void vkt::subgroups::addGeometryShadersFromTemplate (const std::string& spirvTemplate, const vk::SpirVAsmBuildOptions& options, vk::SpirVAsmCollection& collection)
{
	tcu::StringTemplate geometryTemplate(spirvTemplate);

	map<string, string>		linesParams;
	linesParams.insert(pair<string, string>("TOPOLOGY", "InputLines"));

	map<string, string>		pointsParams;
	pointsParams.insert(pair<string, string>("TOPOLOGY", "InputPoints"));

	collection.add("geometry_lines")	<< geometryTemplate.specialize(linesParams)		<< options;
	collection.add("geometry_points")	<< geometryTemplate.specialize(pointsParams)	<< options;
}

void initializeMemory (Context& context, const Allocation& alloc, const subgroups::SSBOData& data)
{
	const vk::VkFormat format = data.format;
	const vk::VkDeviceSize size = data.numElements *
		(data.isImage ? getFormatSizeInBytes(format) : getElementSizeInBytes(format, data.layout));
	if (subgroups::SSBOData::InitializeNonZero == data.initializeType)
	{
		de::Random rnd(context.getTestContext().getCommandLine().getBaseSeed());

		switch (format)
		{
			default:
				DE_FATAL("Illegal buffer format");
				break;
			case VK_FORMAT_R8_SINT:
			case VK_FORMAT_R8G8_SINT:
			case VK_FORMAT_R8G8B8_SINT:
			case VK_FORMAT_R8G8B8A8_SINT:
			case VK_FORMAT_R8_UINT:
			case VK_FORMAT_R8G8_UINT:
			case VK_FORMAT_R8G8B8_UINT:
			case VK_FORMAT_R8G8B8A8_UINT:
			{
				deUint8* ptr = reinterpret_cast<deUint8*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint8)); k++)
				{
					ptr[k] = rnd.getUint8();
				}
			}
			break;
			case VK_FORMAT_R16_SINT:
			case VK_FORMAT_R16G16_SINT:
			case VK_FORMAT_R16G16B16_SINT:
			case VK_FORMAT_R16G16B16A16_SINT:
			case VK_FORMAT_R16_UINT:
			case VK_FORMAT_R16G16_UINT:
			case VK_FORMAT_R16G16B16_UINT:
			case VK_FORMAT_R16G16B16A16_UINT:
			{
				deUint16* ptr = reinterpret_cast<deUint16*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint16)); k++)
				{
					ptr[k] = rnd.getUint16();
				}
			}
			break;
			case VK_FORMAT_R8_USCALED:
			case VK_FORMAT_R8G8_USCALED:
			case VK_FORMAT_R8G8B8_USCALED:
			case VK_FORMAT_R8G8B8A8_USCALED:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint32)); k++)
				{
					deUint32 r = rnd.getUint32();
					ptr[k] = (r & 1) ? r : 0;
				}
			}
			break;
			case VK_FORMAT_R32_SINT:
			case VK_FORMAT_R32G32_SINT:
			case VK_FORMAT_R32G32B32_SINT:
			case VK_FORMAT_R32G32B32A32_SINT:
			case VK_FORMAT_R32_UINT:
			case VK_FORMAT_R32G32_UINT:
			case VK_FORMAT_R32G32B32_UINT:
			case VK_FORMAT_R32G32B32A32_UINT:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint32)); k++)
				{
					ptr[k] = rnd.getUint32();
				}
			}
			break;
			case VK_FORMAT_R64_SINT:
			case VK_FORMAT_R64G64_SINT:
			case VK_FORMAT_R64G64B64_SINT:
			case VK_FORMAT_R64G64B64A64_SINT:
			case VK_FORMAT_R64_UINT:
			case VK_FORMAT_R64G64_UINT:
			case VK_FORMAT_R64G64B64_UINT:
			case VK_FORMAT_R64G64B64A64_UINT:
			{
				deUint64* ptr = reinterpret_cast<deUint64*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint64)); k++)
				{
					ptr[k] = rnd.getUint64();
				}
			}
			break;
			case VK_FORMAT_R16_SFLOAT:
			case VK_FORMAT_R16G16_SFLOAT:
			case VK_FORMAT_R16G16B16_SFLOAT:
			case VK_FORMAT_R16G16B16A16_SFLOAT:
			{
				deFloat16* ptr = reinterpret_cast<deFloat16*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deFloat16)); k++)
				{
					ptr[k] = deFloat32To16(rnd.getFloat());
				}
			}
			break;
			case VK_FORMAT_R32_SFLOAT:
			case VK_FORMAT_R32G32_SFLOAT:
			case VK_FORMAT_R32G32B32_SFLOAT:
			case VK_FORMAT_R32G32B32A32_SFLOAT:
			{
				float* ptr = reinterpret_cast<float*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(float)); k++)
				{
					ptr[k] = rnd.getFloat();
				}
			}
			break;
			case VK_FORMAT_R64_SFLOAT:
			case VK_FORMAT_R64G64_SFLOAT:
			case VK_FORMAT_R64G64B64_SFLOAT:
			case VK_FORMAT_R64G64B64A64_SFLOAT:
			{
				double* ptr = reinterpret_cast<double*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(double)); k++)
				{
					ptr[k] = rnd.getDouble();
				}
			}
			break;
		}
	}
	else if (subgroups::SSBOData::InitializeZero == data.initializeType)
	{
		deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

		for (vk::VkDeviceSize k = 0; k < size / 4; k++)
		{
			ptr[k] = 0;
		}
	}

	if (subgroups::SSBOData::InitializeNone != data.initializeType)
	{
		flushAlloc(context.getDeviceInterface(), context.getDevice(), alloc);
	}
}

deUint32 getResultBinding (const VkShaderStageFlagBits shaderStage)
{
	switch(shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return 0u;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return 1u;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return 2u;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return 3u;
		default:
			DE_ASSERT(0);
			return -1;
	}
	DE_ASSERT(0);
	return -1;
}

tcu::TestStatus vkt::subgroups::makeTessellationEvaluationFrameBufferTest (Context&					context,
																		   VkFormat					format,
																		   const SSBOData*			extraData,
																		   deUint32					extraDataCount,
																		   const void*				internalData,
																		   subgroups::CheckResult	checkResult,
																		   const VkShaderStageFlags	shaderStage)
{
	return makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize(context, format, extraData, extraDataCount, internalData, checkResult, shaderStage, 0u, 0u);
}

tcu::TestStatus vkt::subgroups::makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize (Context&					context,
																							   VkFormat					format,
																							   const SSBOData*			extraData,
																							   deUint32					extraDataCount,
																							   const void*				internalData,
																							   subgroups::CheckResult	checkResult,
																							   const VkShaderStageFlags	shaderStage,
																							   const deUint32			tessShaderStageCreateFlags,
																							   const deUint32			requiredSubgroupSize)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const deUint32							maxWidth				= getMaxWidth();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;
	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;
	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			teCtrlShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("tesc"), 0u));
	const Unique<VkShaderModule>			teEvalShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("tese"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));
	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											//  deUint32			binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	//  deUint32			stride;
		VK_VERTEX_INPUT_RATE_VERTEX					//  VkVertexInputRate	inputRate;
	};
	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,									//  deUint32	location;
		0u,									//  deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,		//  VkFormat	format;
		0u									//  deUint32	offset;
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, shaderStage, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));

	const deUint32 requiredSubgroupSizes[5] = {0u,
											   ((shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? requiredSubgroupSize : 0u),
											   ((shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? requiredSubgroupSize : 0u),
											   0u,
											   0u};

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
																						  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
																						  *vertexShaderModule, *fragmentShaderModule, DE_NULL, *teCtrlShaderModule, *teEvalShaderModule,
																						  *renderPass, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, &vertexInputBinding, &vertexInputAttribute, true, format,
																						  0u, ((shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? tessShaderStageCreateFlags : 0u),
																						  ((shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? tessShaderStageCreateFlags : 0u),
																						  0u, 0u, requiredSubgroupSize != 0u ? requiredSubgroupSizes : DE_NULL));

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device,
							VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(vk, device, queueFamilyIndex));
	const deUint32							subgroupSize			= getSubgroupSize(context);
	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));
	const vk::VkDeviceSize					vertexBufferSize		= 2ull * maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(2u * maxWidth, Vec4(1.0f, 0.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < data.size(); ndx+=2u)
		{
			data[ndx][0] = leftHandPosition;
			leftHandPosition += pixelSize;
			data[ndx+1][0] = leftHandPosition;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], data.size() * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(vk, device, *renderPass, discardableImage.getImageView(), maxWidth, 1u));
	const VkViewport			viewport			= makeViewport(maxWidth, 1u);
	const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
	const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
	Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		beginCommandBuffer(vk, *cmdBuffer);
		{

			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);
			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);
			vk.cmdDraw(*cmdBuffer, 2 * width, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(internalData, datas, width/2u, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

		context.getTestContext().getLog()
				<< TestLog::Message << valuesPassed << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

bool vkt::subgroups::check (std::vector<const void*> datas, deUint32 width, deUint32 ref)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 n = 0; n < width; ++n)
	{
		if (data[n] != ref)
		{
			return false;
		}
	}

	return true;
}

bool vkt::subgroups::checkCompute (std::vector<const void*>		datas,
								   const deUint32				numWorkgroups[3],
								   const deUint32				localSize[3],
								   deUint32						ref)
{
	const deUint32 globalSizeX = numWorkgroups[0] * localSize[0];
	const deUint32 globalSizeY = numWorkgroups[1] * localSize[1];
	const deUint32 globalSizeZ = numWorkgroups[2] * localSize[2];

	return check(datas, globalSizeX * globalSizeY * globalSizeZ, ref);
}

tcu::TestStatus vkt::subgroups::makeGeometryFrameBufferTest (Context&				context,
															 VkFormat				format,
															 const SSBOData*		extraData,
															 deUint32				extraDataCount,
															 const void*			internalData,
															 subgroups::CheckResult	checkResult)
{
	return makeGeometryFrameBufferTestRequiredSubgroupSize(context, format, extraData, extraDataCount, internalData, checkResult, 0u, 0u);
}

tcu::TestStatus vkt::subgroups::makeGeometryFrameBufferTestRequiredSubgroupSize (Context&					context,
																				 VkFormat					format,
																				 const SSBOData*			extraData,
																				 deUint32					extraDataCount,
																				 const void*				internalData,
																				 subgroups::CheckResult		checkResult,
																				 const deUint32				geometryShaderStageCreateFlags,
																				 const deUint32				requiredSubgroupSize)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const deUint32							maxWidth				= getMaxWidth();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;
	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;
	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			geometryShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("geometry"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));
	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											//  deUint32			binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	//  deUint32			stride;
		VK_VERTEX_INPUT_RATE_VERTEX					//  VkVertexInputRate	inputRate;
	};
	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,									//  deUint32	location;
		0u,									//  deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,		//  VkFormat	format;
		0u									//  deUint32	offset;
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, VK_SHADER_STAGE_GEOMETRY_BIT, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));

	const deUint32 requiredSubgroupSizes[5] = {0u, 0u, 0u, requiredSubgroupSize, 0u};

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
																						  *vertexShaderModule, *fragmentShaderModule, *geometryShaderModule, DE_NULL, DE_NULL,
																						  *renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, &vertexInputBinding, &vertexInputAttribute, true, format,
																						  0u, 0u, 0u, geometryShaderStageCreateFlags, 0u,
																						  requiredSubgroupSize != 0u ? requiredSubgroupSizes : DE_NULL));

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device,
							VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(vk, device, queueFamilyIndex));
	const deUint32							subgroupSize			= getSubgroupSize(context);
	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));
	const vk::VkDeviceSize					vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(maxWidth, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], maxWidth * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(vk, device, *renderPass, discardableImage.getImageView(), maxWidth, 1u));
	const VkViewport			viewport			= makeViewport(maxWidth, 1u);
	const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
	const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
	Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			const Allocation& alloc = inputBuffers[ndx]->getAllocation();
			initializeMemory(context, alloc, extraData[ndx]);
		}

		beginCommandBuffer(vk, *cmdBuffer);
		{
			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, width, 1u, 0u, 0u);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(internalData, datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

		context.getTestContext().getLog()
				<< TestLog::Message << valuesPassed << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;

		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

vk::VkShaderStageFlags vkt::subgroups::getPossibleGraphicsSubgroupStages (Context& context, const vk::VkShaderStageFlags testedStages)
{
	const VkPhysicalDeviceSubgroupProperties&	subgroupProperties	= context.getSubgroupProperties();
	VkShaderStageFlags							stages				= testedStages & subgroupProperties.supportedStages;

	DE_ASSERT(isAllGraphicsStages(testedStages));

	if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
	{
		if ((stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		else
			stages = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	if (static_cast<VkShaderStageFlags>(0u) == stages)
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

	return stages;
}

tcu::TestStatus vkt::subgroups::allStages (Context&						context,
										   vk::VkFormat					format,
										   const SSBOData*				extraData,
										   deUint32						extraDataCount,
										   const void*					internalData,
										   const VerificationFunctor&	checkResult,
										   const vk::VkShaderStageFlags	shaderStage)
{
	return vkt::subgroups::allStagesRequiredSubgroupSize(context, format, extraData, extraDataCount, internalData, checkResult, shaderStage,
														 0u, 0u, 0u, 0u, 0u, DE_NULL);
}

tcu::TestStatus vkt::subgroups::allStagesRequiredSubgroupSize (Context&						context,
															   vk::VkFormat					format,
															   const SSBOData*				extraDatas,
															   deUint32						extraDatasCount,
															   const void*					internalData,
															   const VerificationFunctor&	checkResult,
															   const vk::VkShaderStageFlags	shaderStageTested,
															   const deUint32				vertexShaderStageCreateFlags,
															   const deUint32				tessellationControlShaderStageCreateFlags,
															   const deUint32				tessellationEvalShaderStageCreateFlags,
															   const deUint32				geometryShaderStageCreateFlags,
															   const deUint32				fragmentShaderStageCreateFlags,
															   const deUint32				requiredSubgroupSize[5])
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	const deUint32					maxWidth			= getMaxWidth();
	vector<VkShaderStageFlagBits>	stagesVector;
	VkShaderStageFlags				shaderStageRequired	= (VkShaderStageFlags)0ull;

	Move<VkShaderModule>			vertexShaderModule;
	Move<VkShaderModule>			teCtrlShaderModule;
	Move<VkShaderModule>			teEvalShaderModule;
	Move<VkShaderModule>			geometryShaderModule;
	Move<VkShaderModule>			fragmentShaderModule;

	if (shaderStageTested & VK_SHADER_STAGE_VERTEX_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_VERTEX_BIT);
	}
	if (shaderStageTested & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (shaderStageTested & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	}
	if (shaderStageTested & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_GEOMETRY_BIT);
		const VkShaderStageFlags required = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |=  (shaderStageTested & required) ? (VkShaderStageFlags) 0 : required;
	}
	if (shaderStageTested & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		const VkShaderStageFlags required = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |=  (shaderStageTested & required) ? (VkShaderStageFlags) 0 : required;
	}

	const deUint32	stagesCount	= static_cast<deUint32>(stagesVector.size());
	const string	vert		= (shaderStageRequired & VK_SHADER_STAGE_VERTEX_BIT)					? "vert_noSubgroup"		: "vert";
	const string	tesc		= (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)		? "tesc_noSubgroup"		: "tesc";
	const string	tese		= (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)	? "tese_noSubgroup"		: "tese";

	shaderStageRequired = shaderStageTested | shaderStageRequired;

	vertexShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(vert), 0u);
	if (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		teCtrlShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(tesc), 0u);
		teEvalShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(tese), 0u);
	}
	if (shaderStageRequired & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		if (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			// tessellation shaders output line primitives
			geometryShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("geometry_lines"), 0u);
		}
		else
		{
			// otherwise points are processed by geometry shader
			geometryShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("geometry_points"), 0u);
		}
	}
	if (shaderStageRequired & VK_SHADER_STAGE_FRAGMENT_BIT)
		fragmentShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u);

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(stagesCount + extraDatasCount);

	DescriptorSetLayoutBuilder layoutBuilder;
	// The implicit result SSBO we use to store our outputs from the shader
	for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
	{
		const VkDeviceSize shaderSize = (stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? maxWidth * 2 : maxWidth;
		const VkDeviceSize size = getElementSizeInBytes(format, SSBOData::LayoutStd430) * shaderSize;
		inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));

		layoutBuilder.addIndexedBinding(inputBuffers[ndx]->getType(), 1, stagesVector[ndx], getResultBinding(stagesVector[ndx]), DE_NULL);
	}

	for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
	{
		const deUint32 datasNdx = ndx - stagesCount;
		if (extraDatas[datasNdx].isImage)
		{
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraDatas[datasNdx].numElements), 1, extraDatas[datasNdx].format));
		}
		else
		{
			const vk::VkDeviceSize size = getElementSizeInBytes(extraDatas[datasNdx].format, extraDatas[datasNdx].layout) * extraDatas[datasNdx].numElements;
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[ndx]->getAllocation();
		initializeMemory(context, alloc, extraDatas[datasNdx]);

		layoutBuilder.addIndexedBinding(inputBuffers[ndx]->getType(), 1,
								extraDatas[datasNdx].stages, extraDatas[datasNdx].binding, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(vk, device, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, format));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
														   shaderStageRequired,
														   *vertexShaderModule, *fragmentShaderModule, *geometryShaderModule, *teCtrlShaderModule, *teEvalShaderModule,
														   *renderPass,
														   (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
														   DE_NULL, DE_NULL, false, VK_FORMAT_R32G32B32A32_SFLOAT,
														   vertexShaderStageCreateFlags, tessellationControlShaderStageCreateFlags, tessellationEvalShaderStageCreateFlags,
														   geometryShaderStageCreateFlags, fragmentShaderStageCreateFlags, requiredSubgroupSize));

	Move <VkDescriptorPool>	descriptorPool;
	Move <VkDescriptorSet>	descriptorSet;

	if (inputBuffers.size() > 0)
	{
		DescriptorPoolBuilder poolBuilder;

		for (deUint32 ndx = 0u; ndx < static_cast<deUint32>(inputBuffers.size()); ndx++)
		{
			poolBuilder.addType(inputBuffers[ndx]->getType());
		}

		descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		// Create descriptor set
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

		DescriptorSetUpdateBuilder updateBuilder;

		for (deUint32 ndx = 0u; ndx < stagesCount + extraDatasCount; ndx++)
		{
			deUint32 binding;
			if (ndx < stagesCount) binding = getResultBinding(stagesVector[ndx]);
			else binding = extraDatas[ndx -stagesCount].binding;

			if (inputBuffers[ndx]->isImage())
			{
				VkDescriptorImageInfo info =
					makeDescriptorImageInfo(inputBuffers[ndx]->getAsImage()->getSampler(),
											inputBuffers[ndx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(	*descriptorSet,
											DescriptorSetUpdateBuilder::Location::binding(binding),
											inputBuffers[ndx]->getType(), &info);
			}
			else
			{
				VkDescriptorBufferInfo info =
					makeDescriptorBufferInfo(inputBuffers[ndx]->getAsBuffer()->getBuffer(),
							0ull, inputBuffers[ndx]->getAsBuffer()->getSize());

				updateBuilder.writeSingle(	*descriptorSet,
													DescriptorSetUpdateBuilder::Location::binding(binding),
													inputBuffers[ndx]->getType(), &info);
			}
		}

		updateBuilder.update(vk, device);
	}

	{
		const VkQueue					queue					= context.getUniversalQueue();
		const deUint32					queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
		const Unique<VkCommandPool>		cmdPool					(makeCommandPool(vk, device, queueFamilyIndex));
		const deUint32					subgroupSize			= getSubgroupSize(context);
		const Unique<VkCommandBuffer>	cmdBuffer				(makeCommandBuffer(context, *cmdPool));
		unsigned						totalIterations			= 0u;
		unsigned						failedIterations		= 0u;
		Image							resultImage				(context, maxWidth, 1, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer(vk, device, *renderPass, resultImage.getImageView(), maxWidth, 1u));
		const VkViewport				viewport				= makeViewport(maxWidth, 1u);
		const VkRect2D					scissor					= makeRect2D(maxWidth, 1u);
		const vk::VkDeviceSize			imageResultSize			= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
		Buffer							imageBufferResult		(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkImageSubresourceRange	subresourceRange		=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,											//VkImageAspectFlags	aspectMask
			0u,																	//deUint32				baseMipLevel
			1u,																	//deUint32				levelCount
			0u,																	//deUint32				baseArrayLayer
			1u																	//deUint32				layerCount
		};

		const VkImageMemoryBarrier		colorAttachmentBarrier	= makeImageMemoryBarrier(
			(VkAccessFlags)0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			resultImage.getImage(), subresourceRange);

		for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
		{
			for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
			{
				// re-init the data
				const Allocation& alloc = inputBuffers[ndx]->getAllocation();
				initializeMemory(context, alloc, extraDatas[ndx - stagesCount]);
			}

			totalIterations++;

			beginCommandBuffer(vk, *cmdBuffer);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &colorAttachmentBarrier);

			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (stagesCount + extraDatasCount > 0)
				vk.cmdBindDescriptorSets(*cmdBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
						&descriptorSet.get(), 0u, DE_NULL);

			vk.cmdDraw(*cmdBuffer, width, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, resultImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(width, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
			{
				std::vector<const void*> datas;
				if (!inputBuffers[ndx]->isImage())
				{
					const Allocation& resultAlloc = inputBuffers[ndx]->getAllocation();
					invalidateAlloc(vk, device, resultAlloc);
					// we always have our result data first
					datas.push_back(resultAlloc.getHostPtr());
				}

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if ((stagesVector[ndx] & extraDatas[datasNdx].stages) && (!inputBuffers[index]->isImage()))
					{
						const Allocation& resultAlloc = inputBuffers[index]->getAllocation();
						invalidateAlloc(vk, device, resultAlloc);
						// we always have our result data first
						datas.push_back(resultAlloc.getHostPtr());
					}
				}

				// Any stage in the vertex pipeline may be called multiple times per vertex, so we may need >= non-strict comparisons.
				const bool		multiCall	= (	stagesVector[ndx] == VK_SHADER_STAGE_VERTEX_BIT						||
												stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT		||
												stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT	||
												stagesVector[ndx] == VK_SHADER_STAGE_GEOMETRY_BIT					);
				const deUint32	usedWidth	= ((stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? width * 2 : width);

				if (!checkResult(internalData, datas, usedWidth, subgroupSize, multiCall))
					failedIterations++;
			}
			if (shaderStageTested & VK_SHADER_STAGE_FRAGMENT_BIT)
			{
				std::vector<const void*> datas;
				const Allocation& resultAlloc = imageBufferResult.getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if (VK_SHADER_STAGE_FRAGMENT_BIT & extraDatas[datasNdx].stages && (!inputBuffers[index]->isImage()))
					{
						const Allocation& alloc = inputBuffers[index]->getAllocation();
						invalidateAlloc(vk, device, alloc);
						// we always have our result data first
						datas.push_back(alloc.getHostPtr());
					}
				}

				if (!checkResult(internalData, datas, width, subgroupSize, false))
					failedIterations++;
			}

			vk.resetCommandBuffer(*cmdBuffer, 0);
		}

		if (0 < failedIterations)
		{
			unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

			context.getTestContext().getLog()
				<< TestLog::Message << valuesPassed << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;

			return tcu::TestStatus::fail("Failed!");
		}
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeVertexFrameBufferTest (Context&					context,
														   vk::VkFormat				format,
														   const SSBOData*			extraData,
														   deUint32					extraDataCount,
														   const void*				internalData,
														   subgroups::CheckResult	checkResult)
{
	return makeVertexFrameBufferTestRequiredSubgroupSize(context, format, extraData, extraDataCount, internalData, checkResult, 0u, 0u);
}

tcu::TestStatus vkt::subgroups::makeVertexFrameBufferTestRequiredSubgroupSize (Context&					context,
																			   vk::VkFormat				format,
																			   const SSBOData*			extraData,
																			   deUint32					extraDataCount,
																			   const void*				internalData,
																			   subgroups::CheckResult	checkResult,
																			   const deUint32			vertexShaderStageCreateFlags,
																			   const deUint32			requiredSubgroupSize)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							maxWidth				= getMaxWidth();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));
	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};
	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,
		0u,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		0u
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, VK_SHADER_STAGE_VERTEX_BIT, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));

	const deUint32 requiredSubgroupSizes[5] = {requiredSubgroupSize, 0u, 0u, 0u, 0u};
	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
																						  *vertexShaderModule, *fragmentShaderModule,
																						  DE_NULL, DE_NULL, DE_NULL,
																						  *renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
																						  &vertexInputBinding, &vertexInputAttribute, true, format,
																						  vertexShaderStageCreateFlags, 0u, 0u, 0u, 0u,
																						  requiredSubgroupSize != 0u ? requiredSubgroupSizes : DE_NULL));
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;


	for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
	{
		const Allocation& alloc = inputBuffers[ndx]->getAllocation();
		initializeMemory(context, alloc, extraData[ndx]);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}
	updateBuilder.update(vk, device);

	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(vk, device, queueFamilyIndex));

	const deUint32							subgroupSize			= getSubgroupSize(context);

	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));

	const vk::VkDeviceSize					vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;

	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(maxWidth, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], maxWidth * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(vk, device, *renderPass, discardableImage.getImageView(), maxWidth, 1u));
	const VkViewport			viewport			= makeViewport(maxWidth, 1u);
	const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
	const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
	Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			const Allocation& alloc = inputBuffers[ndx]->getAllocation();
			initializeMemory(context, alloc, extraData[ndx]);
		}

		beginCommandBuffer(vk, *cmdBuffer);
		{
			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, width, 1u, 0u, 0u);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(internalData, datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

		context.getTestContext().getLog()
			<< TestLog::Message << valuesPassed << " / "
			<< totalIterations << " values passed" << TestLog::EndMessage;

		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeFragmentFrameBufferTest (Context&				context,
															 VkFormat				format,
															 const SSBOData*		extraDatas,
															 deUint32				extraDatasCount,
															 const void*			internalData,
															 CheckResultFragment	checkResult)
{
	return makeFragmentFrameBufferTestRequiredSubgroupSize(context, format, extraDatas, extraDatasCount, internalData, checkResult, 0u, 0u);
}

tcu::TestStatus vkt::subgroups::makeFragmentFrameBufferTestRequiredSubgroupSize (Context&				context,
																				 VkFormat				format,
																				 const SSBOData*		extraDatas,
																				 deUint32				extraDatasCount,
																				 const void*			internalData,
																				 CheckResultFragment	checkResult,
																				 const deUint32			fragmentShaderStageCreateFlags,
																				 const deUint32			requiredSubgroupSize)
{
	const DeviceInterface&						vk						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();
	const VkQueue								queue					= context.getUniversalQueue();
	const deUint32								queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const Unique<VkShaderModule>				vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>				fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers			(extraDatasCount);

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			const vk::VkDeviceSize	size	= getElementSizeInBytes(extraDatas[i].format, extraDatas[i].layout) * extraDatas[i].numElements;

			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();

		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1, VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));
	const Unique<VkPipelineLayout>		pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkRenderPass>			renderPass(makeRenderPass(context, format));
	const deUint32						requiredSubgroupSizes[5] = {0u, 0u, 0u, 0u, requiredSubgroupSize};
	const Unique<VkPipeline>			pipeline(makeGraphicsPipeline(context,
																	  *pipelineLayout,
																	  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
																	  *vertexShaderModule,
																	  *fragmentShaderModule,
																	  DE_NULL,
																	  DE_NULL,
																	  DE_NULL,
																	  *renderPass,
																	  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
																	  DE_NULL,
																	  DE_NULL,
																	  true,
																	  VK_FORMAT_R32G32B32A32_SFLOAT,
																	  0u,
																	  0u,
																	  0u,
																	  0u,
																	  fragmentShaderStageCreateFlags,
																	  requiredSubgroupSize != 0u ? requiredSubgroupSizes : DE_NULL));
	DescriptorPoolBuilder				poolBuilder;

	// To stop validation complaining, always add at least one type to pool.
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	Move<VkDescriptorPool> descriptorPool;
	// Create descriptor set
	Move<VkDescriptorSet> descriptorSet;

	if (extraDatasCount > 0)
	{
		descriptorPool	= poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		descriptorSet	= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			const VkDescriptorImageInfo info = makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(), inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i), inputBuffers[i]->getType(), &info);
		}
		else
		{
			const VkDescriptorBufferInfo	info	= makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(), 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i), inputBuffers[i]->getType(), &info);
		}
	}

	if (extraDatasCount > 0)
		updateBuilder.update(vk, device);

	const Unique<VkCommandPool>		cmdPool				(makeCommandPool(vk, device, queueFamilyIndex));
	const deUint32					subgroupSize		= getSubgroupSize(context);
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(context, *cmdPool));
	unsigned						totalIterations		= 0;
	unsigned						failedIterations	= 0;

	for (deUint32 width = 8; width <= subgroupSize; width *= 2)
	{
		for (deUint32 height = 8; height <= subgroupSize; height *= 2)
		{
			totalIterations++;

			// re-init the data
			for (deUint32 i = 0; i < extraDatasCount; i++)
			{
				const Allocation& alloc = inputBuffers[i]->getAllocation();

				initializeMemory(context, alloc, extraDatas[i]);
			}

			const VkDeviceSize			formatSize				= getFormatSizeInBytes(format);
			const VkDeviceSize			resultImageSizeInBytes	= width * height * formatSize;
			Image						resultImage				(context, width, height, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			Buffer						resultBuffer			(context, resultImageSizeInBytes, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			const Unique<VkFramebuffer>	framebuffer				(makeFramebuffer(vk, device, *renderPass, resultImage.getImageView(), width, height));
			VkViewport					viewport				= makeViewport(width, height);
			VkRect2D					scissor					= {{0, 0}, {width, height}};

			beginCommandBuffer(vk, *cmdBuffer);

			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, width, height), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDatasCount > 0)
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

			vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, resultImage.getImage(), resultBuffer.getBuffer(), tcu::IVec2(width, height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			std::vector<const void*> datas;
			{
				const Allocation& resultAlloc = resultBuffer.getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}

			if (!checkResult(internalData, datas, width, height, subgroupSize))
			{
				failedIterations++;
			}

			vk.resetCommandBuffer(*cmdBuffer, 0);
		}
	}

	if (0 < failedIterations)
	{
		unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

		context.getTestContext().getLog()
			<< TestLog::Message << valuesPassed << " / "
			<< totalIterations << " values passed" << TestLog::EndMessage;

		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

Move<VkPipeline> makeComputePipeline (Context&					context,
									  const VkPipelineLayout	pipelineLayout,
									  const VkShaderModule		shaderModule,
									  const deUint32			pipelineShaderStageFlags,
									  const deUint32			pipelineCreateFlags,
									  VkPipeline				basePipelineHandle,
									  deUint32					localSizeX,
									  deUint32					localSizeY,
									  deUint32					localSizeZ,
									  deUint32					requiredSubgroupSize)
{
	const deUint32														localSize[3]				= {localSizeX, localSizeY, localSizeZ};
	const vk::VkSpecializationMapEntry									entries[3]					=
	{
		{0, sizeof(deUint32) * 0, sizeof(deUint32)},
		{1, sizeof(deUint32) * 1, sizeof(deUint32)},
		{2, static_cast<deUint32>(sizeof(deUint32) * 2), sizeof(deUint32)},
	};
	const vk::VkSpecializationInfo										info						=
	{
		/* mapEntryCount = */ 3,
		/* pMapEntries   = */ entries,
		/* dataSize      = */ sizeof(localSize),
		/* pData         = */ localSize
	};
	const vk::VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT	subgroupSizeCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,	// VkStructureType    sType;
		DE_NULL,																		// void*              pNext;
		requiredSubgroupSize															// uint32_t           requiredSubgroupSize;
	};
	const vk::VkPipelineShaderStageCreateInfo							pipelineShaderStageParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,				// VkStructureType					sType;
		(requiredSubgroupSize != 0u ? &subgroupSizeCreateInfo : DE_NULL),	// const void*						pNext;
		pipelineShaderStageFlags,											// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,										// VkShaderStageFlagBits			stage;
		shaderModule,														// VkShaderModule					module;
		"main",																// const char*						pName;
		&info,																// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const vk::VkComputePipelineCreateInfo								pipelineCreateInfo			=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType	sType;
		DE_NULL,										// const void*						pNext;
		pipelineCreateFlags,							// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,									// VkPipelineLayout					layout;
		basePipelineHandle,								// VkPipeline						basePipelineHandle;
		-1,												// deInt32							basePipelineIndex;
	};

	return createComputePipeline(context.getDeviceInterface(), context.getDevice(), DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus vkt::subgroups::makeComputeTestRequiredSubgroupSize (Context&			context,
																	 VkFormat			format,
																	 const SSBOData*	inputs,
																	 deUint32			inputsCount,
																	 const void*		internalData,
																	 CheckResultCompute	checkResult,
																	 const deUint32		pipelineShaderStageCreateFlags,
																	 const deUint32		numWorkgroups[3],
																	 const deBool		isRequiredSubgroupSize,
																	 const deUint32		subgroupSize,
																	 const deUint32		localSizesToTest[][3],
																	 const deUint32		localSizesToTestCount)
{
	const DeviceInterface&									vk								= context.getDeviceInterface();
	const VkDevice											device							= context.getDevice();
	const VkQueue											queue							= context.getUniversalQueue();
	const deUint32											queueFamilyIndex				= context.getUniversalQueueFamilyIndex();
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
	const VkDeviceSize										elementSize						= getFormatSizeInBytes(format);
	const VkDeviceSize										maxSubgroupSize					= isRequiredSubgroupSize
																							? deMax32(subgroupSizeControlProperties.maxSubgroupSize, maxSupportedSubgroupSize())
																							: maxSupportedSubgroupSize();
	const VkDeviceSize										resultBufferSize				= maxSubgroupSize * maxSubgroupSize * maxSubgroupSize;
	const VkDeviceSize										resultBufferSizeInBytes			= resultBufferSize * elementSize;
	Buffer													resultBuffer					(context, resultBufferSizeInBytes);
	std::vector< de::SharedPtr<BufferOrImage> >				inputBuffers					(inputsCount);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputs[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(inputs[i].numElements), 1, inputs[i].format));
		}
		else
		{
			const vk::VkDeviceSize size = getElementSizeInBytes(inputs[i].format, inputs[i].layout) * inputs[i].numElements;

			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();

		initializeMemory(context, alloc, inputs[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(
		resultBuffer.getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		layoutBuilder.addBinding(
			inputBuffers[i]->getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(vk, device));

	const Unique<VkShaderModule> shaderModule(
		createShaderModule(vk, device,
						   context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(vk, device, *descriptorSetLayout));

	DescriptorPoolBuilder poolBuilder;

	poolBuilder.addType(resultBuffer.getType());

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool>	descriptorPool			(poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const Unique<VkDescriptorSet>	descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo	resultDescriptorInfo =	makeDescriptorBufferInfo(resultBuffer.getBuffer(), 0ull, resultBufferSizeInBytes);
	DescriptorSetUpdateBuilder		updateBuilder;

	updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			const VkDescriptorImageInfo		info	= makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(), inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i + 1), inputBuffers[i]->getType(), &info);
		}
		else
		{
			vk::VkDeviceSize		size	= getElementSizeInBytes(inputs[i].format, inputs[i].layout) * inputs[i].numElements;
			VkDescriptorBufferInfo	info	= makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(), 0ull, size);

			updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i + 1), inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const Unique<VkCommandPool>						cmdPool				(makeCommandPool(vk, device, queueFamilyIndex));
	unsigned										totalIterations		= 0;
	unsigned										failedIterations	= 0;
	const Unique<VkCommandBuffer>					cmdBuffer			(makeCommandBuffer(context, *cmdPool));
	std::vector<de::SharedPtr<Move<VkPipeline>>>	pipelines			(localSizesToTestCount);

	context.getTestContext().touchWatchdog();
	{
		pipelines[0] = de::SharedPtr<Move<VkPipeline>>(new Move<VkPipeline>(makeComputePipeline(context,
																								*pipelineLayout,
																								*shaderModule,
																								pipelineShaderStageCreateFlags,
																								VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
																								(VkPipeline) DE_NULL,
																								localSizesToTest[0][0],
																								localSizesToTest[0][1],
																								localSizesToTest[0][2],
																								isRequiredSubgroupSize ? subgroupSize : 0u)));
	}
	context.getTestContext().touchWatchdog();

	for (deUint32 index = 1; index < (localSizesToTestCount - 1); index++)
	{
		const deUint32 nextX = localSizesToTest[index][0];
		const deUint32 nextY = localSizesToTest[index][1];
		const deUint32 nextZ = localSizesToTest[index][2];

		context.getTestContext().touchWatchdog();
		{
			pipelines[index] = de::SharedPtr<Move<VkPipeline>>(new Move<VkPipeline>(makeComputePipeline(context,
																										*pipelineLayout,
																										*shaderModule,
																										pipelineShaderStageCreateFlags,
																										VK_PIPELINE_CREATE_DERIVATIVE_BIT,
																										**pipelines[0],
																										nextX,
																										nextY,
																										nextZ,
																										isRequiredSubgroupSize ? subgroupSize : 0u)));
		}
		context.getTestContext().touchWatchdog();
	}

	for (deUint32 index = 0; index < (localSizesToTestCount - 1); index++)
	{
		// we are running one test
		totalIterations++;

		beginCommandBuffer(vk, *cmdBuffer);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **pipelines[index]);

			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

			vk.cmdDispatch(*cmdBuffer,numWorkgroups[0], numWorkgroups[1], numWorkgroups[2]);
		}
		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		std::vector<const void*> datas;

		{
			const Allocation& resultAlloc = resultBuffer.getAllocation();
			invalidateAlloc(vk, device, resultAlloc);

			// we always have our result data first
			datas.push_back(resultAlloc.getHostPtr());
		}

		for (deUint32 i = 0; i < inputsCount; i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(internalData, datas, numWorkgroups, localSizesToTest[index], subgroupSize))
		{
			failedIterations++;
		}

		vk.resetCommandBuffer(*cmdBuffer, 0);
	}

	if (0 < failedIterations)
	{
		unsigned valuesPassed = (failedIterations > totalIterations) ? 0u : (totalIterations - failedIterations);

		context.getTestContext().getLog()
			<< TestLog::Message << valuesPassed << " / "
			<< totalIterations << " values passed" << TestLog::EndMessage;

		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeComputeTest (Context&				context,
												 VkFormat				format,
												 const SSBOData*		inputs,
												 deUint32				inputsCount,
												 const void*			internalData,
												 CheckResultCompute		checkResult,
												 deUint32				requiredSubgroupSize,
												 const deUint32			pipelineShaderStageCreateFlags)
{
	const deUint32 numWorkgroups[3] = {4, 2, 2};
	deUint32 subgroupSize = requiredSubgroupSize;

	if(requiredSubgroupSize == 0)
		subgroupSize = vkt::subgroups::getSubgroupSize(context);

	const deUint32 localSizesToTestCount = 8;
	deUint32 localSizesToTest[localSizesToTestCount][3] =
	{
		{1, 1, 1},
		{subgroupSize, 1, 1},
		{1, subgroupSize, 1},
		{1, 1, subgroupSize},
		{32, 4, 1},
		{1, 4, 32},
		{3, 5, 7},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};

	return makeComputeTestRequiredSubgroupSize(context, format, inputs, inputsCount, internalData, checkResult, pipelineShaderStageCreateFlags,
											   numWorkgroups, requiredSubgroupSize != 0u, subgroupSize, localSizesToTest, localSizesToTestCount);
}

static inline void checkShaderStageSetValidity (const VkShaderStageFlags shaderStages)
{
	if (shaderStages == 0)
		TCU_THROW(InternalError, "Shader stage is not specified");

	// It can actually be only 1 or 0.
	const deUint32 exclusivePipelinesCount	= (isAllComputeStages(shaderStages) ? 1 :0)
											+ (isAllGraphicsStages(shaderStages) ? 1 :0)
											+ (isAllRayTracingStages(shaderStages) ? 1 :0);

	if (exclusivePipelinesCount != 1)
		TCU_THROW(InternalError, "Mix of shaders from different pipelines is detected");
}

void vkt::subgroups::supportedCheckShader (Context& context, const VkShaderStageFlags shaderStages)
{
	checkShaderStageSetValidity(shaderStages);

	if ((context.getSubgroupProperties().supportedStages & shaderStages) == 0)
	{
		if (isAllComputeStages(shaderStages))
			TCU_FAIL("Compute shader is required to support subgroup operations");
		else
			TCU_THROW(NotSupportedError, "Subgroup support is not available for test shader stage(s)");
	}

	if ((VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) & shaderStages &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().tessellationIsolines)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Tessellation iso lines are not supported by this implementation");
	}
}


namespace vkt
{
namespace subgroups
{
typedef std::vector< de::SharedPtr<BufferOrImage> > vectorBufferOrImage;

enum ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,
	CALL_GROUP,
	GROUP_COUNT
};

const std::vector<vk::VkFormat> getAllRayTracingFormats()
{
	std::vector<VkFormat> formats;

	formats.push_back(VK_FORMAT_R8G8B8_SINT);
	formats.push_back(VK_FORMAT_R8_UINT);
	formats.push_back(VK_FORMAT_R8G8B8A8_UINT);
	formats.push_back(VK_FORMAT_R16G16B16_SINT);
	formats.push_back(VK_FORMAT_R16_UINT);
	formats.push_back(VK_FORMAT_R16G16B16A16_UINT);
	formats.push_back(VK_FORMAT_R32G32B32_SINT);
	formats.push_back(VK_FORMAT_R32_UINT);
	formats.push_back(VK_FORMAT_R32G32B32A32_UINT);
	formats.push_back(VK_FORMAT_R64G64B64_SINT);
	formats.push_back(VK_FORMAT_R64_UINT);
	formats.push_back(VK_FORMAT_R64G64B64A64_UINT);
	formats.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);
	formats.push_back(VK_FORMAT_R32_SFLOAT);
	formats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
	formats.push_back(VK_FORMAT_R64_SFLOAT);
	formats.push_back(VK_FORMAT_R64G64B64_SFLOAT);
	formats.push_back(VK_FORMAT_R64G64B64A64_SFLOAT);
	formats.push_back(VK_FORMAT_R8_USCALED);
	formats.push_back(VK_FORMAT_R8G8_USCALED);
	formats.push_back(VK_FORMAT_R8G8B8_USCALED);
	formats.push_back(VK_FORMAT_R8G8B8A8_USCALED);

	return formats;
}

void addRayTracingNoSubgroupShader (SourceCollections& programCollection)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string rgenShaderNoSubgroups =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing: require\n"
		"layout(location = 0) rayPayloadEXT uvec4 payload;\n"
		"layout(location = 0) callableDataEXT uvec4 callData;"
		"layout(set = 1, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  uint  rayFlags   = 0;\n"
		"  uint  cullMask   = 0xFF;\n"
		"  float tmin       = 0.0;\n"
		"  float tmax       = 9.0;\n"
		"  vec3  origin     = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
		"  vec3  directHit  = vec3(0.0, 0.0, -1.0);\n"
		"  vec3  directMiss = vec3(0.0, 0.0, +1.0);\n"
		"\n"
		"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, directHit, tmax, 0);\n"
		"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, directMiss, tmax, 0);\n"
		"  executeCallableEXT(0, 0);"
		"}\n";
	const std::string hitShaderNoSubgroups =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing: require\n"
		"hitAttributeEXT vec3 attribs;\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";
	const std::string missShaderNoSubgroups =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing: require\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";
	const std::string sectShaderNoSubgroups =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing: require\n"
		"hitAttributeEXT vec3 hitAttribute;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  reportIntersectionEXT(0.75f, 0x7Eu);\n"
		"}\n";
	const std::string callShaderNoSubgroups =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing: require\n"
		"layout(location = 0) callableDataInEXT float callData;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	programCollection.glslSources.add("rgen_noSubgroup") << glu::RaygenSource		(rgenShaderNoSubgroups) << buildOptions;
	programCollection.glslSources.add("ahit_noSubgroup") << glu::AnyHitSource		(hitShaderNoSubgroups)  << buildOptions;
	programCollection.glslSources.add("chit_noSubgroup") << glu::ClosestHitSource	(hitShaderNoSubgroups)  << buildOptions;
	programCollection.glslSources.add("miss_noSubgroup") << glu::MissSource			(missShaderNoSubgroups) << buildOptions;
	programCollection.glslSources.add("sect_noSubgroup") << glu::IntersectionSource	(sectShaderNoSubgroups) << buildOptions;
	programCollection.glslSources.add("call_noSubgroup") << glu::CallableSource		(callShaderNoSubgroups) << buildOptions;
}

static vector<VkShaderStageFlagBits> enumerateRayTracingShaderStages (const VkShaderStageFlags	shaderStage)
{
	vector<VkShaderStageFlagBits>	result;
	const VkShaderStageFlagBits		shaderStageFlags[]	=
	{
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_SHADER_STAGE_MISS_BIT_KHR,
		VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		VK_SHADER_STAGE_CALLABLE_BIT_KHR,
	};

	for (auto shaderStageFlag: shaderStageFlags)
	{
		if (0 != (shaderStage & shaderStageFlag))
			result.push_back(shaderStageFlag);
	}

	return result;
}

static deUint32 getRayTracingResultBinding (const VkShaderStageFlagBits shaderStage)
{
	const VkShaderStageFlags	shaderStageFlags[]	=
	{
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_SHADER_STAGE_MISS_BIT_KHR,
		VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		VK_SHADER_STAGE_CALLABLE_BIT_KHR,
	};

	for (deUint32 shaderStageNdx = 0; shaderStageNdx < DE_LENGTH_OF_ARRAY(shaderStageFlags); ++shaderStageNdx)
	{
		if (0 != (shaderStage & shaderStageFlags[shaderStageNdx]))
		{
			DE_ASSERT(0 == (shaderStage & (~shaderStageFlags[shaderStageNdx])));

			return shaderStageNdx;
		}
	}

	TCU_THROW(InternalError, "Non-raytracing stage specified or no stage at all");
}

static vectorBufferOrImage makeRayTracingInputBuffers (Context&								context,
													   VkFormat								format,
													   const SSBOData*						extraDatas,
													   deUint32								extraDatasCount,
													   const vector<VkShaderStageFlagBits>&	stagesVector)
{
	const size_t		stagesCount		= stagesVector.size();
	const VkDeviceSize	shaderSize		= getMaxWidth();
	const VkDeviceSize	inputBufferSize	= getElementSizeInBytes(format, SSBOData::LayoutStd430) * shaderSize;
	vectorBufferOrImage	inputBuffers	(stagesCount + extraDatasCount);

	// The implicit result SSBO we use to store our outputs from the shader
	for (size_t stageNdx = 0u; stageNdx < stagesCount; ++stageNdx)
		inputBuffers[stageNdx]	= de::SharedPtr<BufferOrImage>(new Buffer(context, inputBufferSize));

	for (size_t stageNdx = stagesCount; stageNdx < stagesCount + extraDatasCount; ++stageNdx)
	{
		const size_t	datasNdx	= stageNdx - stagesCount;

		if (extraDatas[datasNdx].isImage)
		{
			inputBuffers[stageNdx] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraDatas[datasNdx].numElements), 1, extraDatas[datasNdx].format));
		}
		else
		{
			const VkDeviceSize size = getElementSizeInBytes(extraDatas[datasNdx].format, extraDatas[datasNdx].layout) * extraDatas[datasNdx].numElements;

			inputBuffers[stageNdx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		initializeMemory(context, inputBuffers[stageNdx]->getAllocation(), extraDatas[datasNdx]);
	}

	return inputBuffers;
}

static Move<VkDescriptorSetLayout> makeRayTracingDescriptorSetLayout (Context&								context,
																	  const SSBOData*						extraDatas,
																	  deUint32								extraDatasCount,
																	  const vector<VkShaderStageFlagBits>&	stagesVector,
																	  const vectorBufferOrImage&			inputBuffers)
{
	const DeviceInterface&		vkd				= context.getDeviceInterface();
	const VkDevice				device			= context.getDevice();
	const size_t				stagesCount		= stagesVector.size();
	DescriptorSetLayoutBuilder	layoutBuilder;

	// The implicit result SSBO we use to store our outputs from the shader
	for (size_t stageNdx = 0u; stageNdx < stagesVector.size(); ++stageNdx)
	{
		const deUint32	stageBinding	= getRayTracingResultBinding(stagesVector[stageNdx]);

		layoutBuilder.addIndexedBinding(inputBuffers[stageNdx]->getType(), 1, stagesVector[stageNdx], stageBinding, DE_NULL);
	}

	for (size_t stageNdx = stagesCount; stageNdx < stagesCount + extraDatasCount; ++stageNdx)
	{
		const size_t datasNdx = stageNdx - stagesCount;

		layoutBuilder.addIndexedBinding(inputBuffers[stageNdx]->getType(), 1, extraDatas[datasNdx].stages, extraDatas[datasNdx].binding, DE_NULL);
	}

	return layoutBuilder.build(vkd, device);
}

static Move<VkDescriptorSetLayout> makeRayTracingDescriptorSetLayoutAS (Context&	context)
{
	const DeviceInterface&		vkd				= context.getDeviceInterface();
	const VkDevice				device			= context.getDevice();
	DescriptorSetLayoutBuilder	layoutBuilder;

	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	return layoutBuilder.build(vkd, device);
}

static Move<VkDescriptorPool> makeRayTracingDescriptorPool (Context&						context,
															const vectorBufferOrImage&		inputBuffers)
{
	const DeviceInterface&	vkd					= context.getDeviceInterface();
	const VkDevice			device				= context.getDevice();
	const deUint32			maxDescriptorSets	= 2u;
	DescriptorPoolBuilder	poolBuilder;
	Move<VkDescriptorPool>	result;

	if (inputBuffers.size() > 0)
	{
		for (size_t ndx = 0u; ndx < inputBuffers.size(); ndx++)
			poolBuilder.addType(inputBuffers[ndx]->getType());
	}

	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

	result = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, maxDescriptorSets);

	return result;
}

static Move<VkDescriptorSet> makeRayTracingDescriptorSet (Context&								context,
														  VkDescriptorPool						descriptorPool,
														  VkDescriptorSetLayout					descriptorSetLayout,
														  const SSBOData*						extraDatas,
														  deUint32								extraDatasCount,
														  const vector<VkShaderStageFlagBits>&	stagesVector,
														  const vectorBufferOrImage&			inputBuffers)
{
	const DeviceInterface&	vkd				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	const size_t			stagesCount		= stagesVector.size();
	Move<VkDescriptorSet>	descriptorSet;

	if (inputBuffers.size() > 0)
	{
		DescriptorSetUpdateBuilder updateBuilder;

		// Create descriptor set
		descriptorSet = makeDescriptorSet(vkd, device, descriptorPool, descriptorSetLayout);

		for (size_t ndx = 0u; ndx < stagesCount + extraDatasCount; ndx++)
		{
			const deUint32	binding	= (ndx < stagesCount)
									? getRayTracingResultBinding(stagesVector[ndx])
									: extraDatas[ndx - stagesCount].binding;

			if (inputBuffers[ndx]->isImage())
			{
				const VkDescriptorImageInfo		info	= makeDescriptorImageInfo(inputBuffers[ndx]->getAsImage()->getSampler(), inputBuffers[ndx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(binding), inputBuffers[ndx]->getType(), &info);
			}
			else
			{
				const VkDescriptorBufferInfo	info	= makeDescriptorBufferInfo(inputBuffers[ndx]->getAsBuffer()->getBuffer(), 0ull, inputBuffers[ndx]->getAsBuffer()->getSize());

				updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(binding), inputBuffers[ndx]->getType(), &info);
			}
		}

		updateBuilder.update(vkd, device);
	}

	return descriptorSet;
}

static Move<VkDescriptorSet> makeRayTracingDescriptorSetAS (Context&									context,
															VkDescriptorPool							descriptorPool,
															VkDescriptorSetLayout						descriptorSetLayout,
															de::MovePtr<TopLevelAccelerationStructure>&	topLevelAccelerationStructure)
{
	const DeviceInterface&								vkd										= context.getDeviceInterface();
	const VkDevice										device									= context.getDevice();
	const TopLevelAccelerationStructure*				topLevelAccelerationStructurePtr		= topLevelAccelerationStructure.get();
	const VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};
	Move<VkDescriptorSet>								descriptorSet = makeDescriptorSet(vkd, device, descriptorPool, descriptorSetLayout);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	return descriptorSet;
}

static Move<VkPipelineLayout> makeRayTracingPipelineLayout (Context&					context,
															const VkDescriptorSetLayout	descriptorSetLayout0,
															const VkDescriptorSetLayout	descriptorSetLayout1)
{
	const DeviceInterface&						vkd							= context.getDeviceInterface();
	const VkDevice								device						= context.getDevice();
	const std::vector<VkDescriptorSetLayout>	descriptorSetLayouts		{ descriptorSetLayout0, descriptorSetLayout1 };
	const deUint32								descriptorSetLayoutsSize	= static_cast<deUint32>(descriptorSetLayouts.size());

	return makePipelineLayout(vkd, device, descriptorSetLayoutsSize, descriptorSetLayouts.data());
}

static de::MovePtr<TopLevelAccelerationStructure> createTopAccelerationStructure (Context&											context,
																				  de::SharedPtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure)
{
	const DeviceInterface&						vkd			= context.getDeviceInterface();
	const VkDevice								device		= context.getDevice();
	Allocator&									allocator	= context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();

	result->setInstanceCount(1);
	result->addInstance(bottomLevelAccelerationStructure);
	result->create(vkd, device, allocator);

	return result;
}

static de::SharedPtr<BottomLevelAccelerationStructure> createBottomAccelerationStructure (Context&	context)
{
	const DeviceInterface&							vkd				= context.getDeviceInterface();
	const VkDevice									device			= context.getDevice();
	Allocator&										allocator		= context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result			= makeBottomLevelAccelerationStructure();
	const std::vector<tcu::Vec3>					geometryData	{ tcu::Vec3(-1.0f, -1.0f, -2.0f), tcu::Vec3(+1.0f, +1.0f, -1.0f) };

	result->setGeometryCount(1u);
	result->addGeometry(geometryData, false);
	result->create(vkd, device, allocator, 0u);

	return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

static de::MovePtr<RayTracingPipeline> makeRayTracingPipeline (Context&					context,
															   const VkShaderStageFlags	shaderStageTested,
															   const VkPipelineLayout	pipelineLayout,
															   const deUint32			shaderStageCreateFlags[6],
															   const deUint32			requiredSubgroupSize[6],
															   Move<VkPipeline>&		pipelineOut)
{
	const DeviceInterface&											vkd									= context.getDeviceInterface();
	const VkDevice													device								= context.getDevice();
	BinaryCollection&												collection							= context.getBinaryCollection();
	const char*														shaderRgenName						= (0 != (shaderStageTested & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			? "rgen" : "rgen_noSubgroup";
	const char*														shaderAhitName						= (0 != (shaderStageTested & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			? "ahit" : "ahit_noSubgroup";
	const char*														shaderChitName						= (0 != (shaderStageTested & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		? "chit" : "chit_noSubgroup";
	const char*														shaderMissName						= (0 != (shaderStageTested & VK_SHADER_STAGE_MISS_BIT_KHR))				? "miss" : "miss_noSubgroup";
	const char*														shaderSectName						= (0 != (shaderStageTested & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))		? "sect" : "sect_noSubgroup";
	const char*														shaderCallName						= (0 != (shaderStageTested & VK_SHADER_STAGE_CALLABLE_BIT_KHR))			? "call" : "call_noSubgroup";
	const VkShaderModuleCreateFlags									noShaderModuleCreateFlags			= static_cast<VkShaderModuleCreateFlags>(0);
	Move<VkShaderModule>											rgenShaderModule					= createShaderModule(vkd, device, collection.get(shaderRgenName), noShaderModuleCreateFlags);
	Move<VkShaderModule>											ahitShaderModule					= createShaderModule(vkd, device, collection.get(shaderAhitName), noShaderModuleCreateFlags);
	Move<VkShaderModule>											chitShaderModule					= createShaderModule(vkd, device, collection.get(shaderChitName), noShaderModuleCreateFlags);
	Move<VkShaderModule>											missShaderModule					= createShaderModule(vkd, device, collection.get(shaderMissName), noShaderModuleCreateFlags);
	Move<VkShaderModule>											sectShaderModule					= createShaderModule(vkd, device, collection.get(shaderSectName), noShaderModuleCreateFlags);
	Move<VkShaderModule>											callShaderModule					= createShaderModule(vkd, device, collection.get(shaderCallName), noShaderModuleCreateFlags);
	const VkPipelineShaderStageCreateFlags							noPipelineShaderStageCreateFlags	= static_cast<VkPipelineShaderStageCreateFlags>(0);
	const VkPipelineShaderStageCreateFlags							rgenPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[0];
	const VkPipelineShaderStageCreateFlags							ahitPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[1];
	const VkPipelineShaderStageCreateFlags							chitPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[2];
	const VkPipelineShaderStageCreateFlags							missPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[3];
	const VkPipelineShaderStageCreateFlags							sectPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[4];
	const VkPipelineShaderStageCreateFlags							callPipelineShaderStageCreateFlags	= (shaderStageCreateFlags == DE_NULL) ? noPipelineShaderStageCreateFlags : shaderStageCreateFlags[5];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT	requiredSubgroupSizeCreateInfo[6]	=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[0] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[1] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[2] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[3] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[4] : 0u,
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
			DE_NULL,
			requiredSubgroupSize != DE_NULL ? requiredSubgroupSize[5] : 0u,
		},
	};
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	rgenRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[0].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[0];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	ahitRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[1].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[1];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	chitRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[2].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[2];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	missRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[3].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[3];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	sectRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[4].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[4];
	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT*	callRequiredSubgroupSizeCreateInfo	= (requiredSubgroupSizeCreateInfo[5].requiredSubgroupSize == 0) ? DE_NULL : &requiredSubgroupSizeCreateInfo[5];
	de::MovePtr<RayTracingPipeline>									rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR		, rgenShaderModule, RAYGEN_GROUP,	DE_NULL, rgenPipelineShaderStageCreateFlags, rgenRequiredSubgroupSizeCreateInfo);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR		, ahitShaderModule, HIT_GROUP,		DE_NULL, ahitPipelineShaderStageCreateFlags, ahitRequiredSubgroupSizeCreateInfo);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	, chitShaderModule, HIT_GROUP,		DE_NULL, chitPipelineShaderStageCreateFlags, chitRequiredSubgroupSizeCreateInfo);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, missShaderModule, MISS_GROUP,		DE_NULL, missPipelineShaderStageCreateFlags, missRequiredSubgroupSizeCreateInfo);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, sectShaderModule, HIT_GROUP,		DE_NULL, sectPipelineShaderStageCreateFlags, sectRequiredSubgroupSizeCreateInfo);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, callShaderModule, CALL_GROUP,		DE_NULL, callPipelineShaderStageCreateFlags, callRequiredSubgroupSizeCreateInfo);

	// Must execute createPipeline here, due to pNext pointers in calls to addShader are local
	pipelineOut	= rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return rayTracingPipeline;
}

VkShaderStageFlags getPossibleRayTracingSubgroupStages (Context& context, const VkShaderStageFlags testedStages)
{
	const VkPhysicalDeviceSubgroupProperties&	subgroupProperties	= context.getSubgroupProperties();
	const VkShaderStageFlags					stages				= testedStages & subgroupProperties.supportedStages;

	DE_ASSERT(isAllRayTracingStages(testedStages));

	return stages;
}

tcu::TestStatus allRayTracingStages (Context&						context,
									 VkFormat						format,
									 const SSBOData*				extraDatas,
									 deUint32						extraDataCount,
									 const void*					internalData,
									 const VerificationFunctor&		checkResult,
									 const VkShaderStageFlags		shaderStage)
{
	return vkt::subgroups::allRayTracingStagesRequiredSubgroupSize(context,
																   format,
																   extraDatas,
																   extraDataCount,
																   internalData,
																   checkResult,
																   shaderStage,
																   DE_NULL,
																   DE_NULL);
}

tcu::TestStatus allRayTracingStagesRequiredSubgroupSize (Context&					context,
														 VkFormat					format,
														 const SSBOData*			extraDatas,
														 deUint32					extraDatasCount,
														 const void*				internalData,
														 const VerificationFunctor&	checkResult,
														 const VkShaderStageFlags	shaderStageTested,
														 const deUint32				shaderStageCreateFlags[6],
														 const deUint32				requiredSubgroupSize[6])
{
	const DeviceInterface&							vkd									= context.getDeviceInterface();
	const VkDevice									device								= context.getDevice();
	const VkQueue									queue								= context.getUniversalQueue();
	const deUint32									queueFamilyIndex					= context.getUniversalQueueFamilyIndex();
	Allocator&										allocator							= context.getDefaultAllocator();
	const deUint32									subgroupSize						= getSubgroupSize(context);
	const deUint32									maxWidth							= getMaxWidth();
	const vector<VkShaderStageFlagBits>				stagesVector						= enumerateRayTracingShaderStages(shaderStageTested);
	const deUint32									stagesCount							= static_cast<deUint32>(stagesVector.size());
	de::SharedPtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= createBottomAccelerationStructure(context);
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure		= createTopAccelerationStructure(context, bottomLevelAccelerationStructure);
	vectorBufferOrImage								inputBuffers						= makeRayTracingInputBuffers(context, format, extraDatas, extraDatasCount, stagesVector);
	const Move<VkDescriptorSetLayout>				descriptorSetLayout					= makeRayTracingDescriptorSetLayout(context, extraDatas, extraDatasCount, stagesVector, inputBuffers);
	const Move<VkDescriptorSetLayout>				descriptorSetLayoutAS				= makeRayTracingDescriptorSetLayoutAS(context);
	const Move<VkPipelineLayout>					pipelineLayout						= makeRayTracingPipelineLayout(context, *descriptorSetLayout, *descriptorSetLayoutAS);
	Move<VkPipeline>								pipeline							= Move<VkPipeline>();
	const de::MovePtr<RayTracingPipeline>			rayTracingPipeline					= makeRayTracingPipeline(context, shaderStageTested, *pipelineLayout, shaderStageCreateFlags, requiredSubgroupSize, pipeline);
	const deUint32									shaderGroupHandleSize				= context.getRayTracingPipelineProperties().shaderGroupHandleSize;
	const deUint32									shaderGroupBaseAlignment			= context.getRayTracingPipelineProperties().shaderGroupBaseAlignment;
	de::MovePtr<BufferWithMemory>					rgenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, RAYGEN_GROUP, 1u);
	de::MovePtr<BufferWithMemory>					missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, MISS_GROUP,   1u);
	de::MovePtr<BufferWithMemory>					hitsShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, HIT_GROUP,    1u);
	de::MovePtr<BufferWithMemory>					callShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, CALL_GROUP,   1u);
	const VkStridedDeviceAddressRegionKHR			rgenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, rgenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR			hitsShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitsShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR			callShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const Move<VkDescriptorPool>					descriptorPool						= makeRayTracingDescriptorPool(context, inputBuffers);
	const Move<VkDescriptorSet>						descriptorSet						= makeRayTracingDescriptorSet(context, *descriptorPool, *descriptorSetLayout, extraDatas, extraDatasCount, stagesVector, inputBuffers);
	const Move<VkDescriptorSet>						descriptorSetAS						= makeRayTracingDescriptorSetAS(context, *descriptorPool, *descriptorSetLayoutAS, topLevelAccelerationStructure);
	const Move<VkCommandPool>						cmdPool								= makeCommandPool(vkd, device, queueFamilyIndex);
	const Move<VkCommandBuffer>						cmdBuffer							= makeCommandBuffer(context, *cmdPool);
	deUint32										passIterations						= 0u;
	deUint32										failIterations						= 0u;

	DE_ASSERT(shaderStageTested != 0);

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{

		for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
		{
			// re-init the data
			const Allocation& alloc = inputBuffers[ndx]->getAllocation();

			initializeMemory(context, alloc, extraDatas[ndx - stagesCount]);
		}

		beginCommandBuffer(vkd, *cmdBuffer);
		{
			vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

			bottomLevelAccelerationStructure->build(vkd, device, *cmdBuffer);
			topLevelAccelerationStructure->build(vkd, device, *cmdBuffer);

			vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 1u, 1u, &descriptorSetAS.get(), 0u, DE_NULL);

			if (stagesCount + extraDatasCount > 0)
				vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

			cmdTraceRays(vkd,
				*cmdBuffer,
				&rgenShaderBindingTableRegion,
				&missShaderBindingTableRegion,
				&hitsShaderBindingTableRegion,
				&callShaderBindingTableRegion,
				width, 1, 1);

			const VkMemoryBarrier	postTraceMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
			cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, &postTraceMemoryBarrier);
		}
		endCommandBuffer(vkd, *cmdBuffer);

		submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

		for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
		{
			std::vector<const void*> datas;

			if (!inputBuffers[ndx]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[ndx]->getAllocation();

				invalidateAlloc(vkd, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}

			for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
			{
				const deUint32 datasNdx = index - stagesCount;

				if ((stagesVector[ndx] & extraDatas[datasNdx].stages) && (!inputBuffers[index]->isImage()))
				{
					const Allocation& resultAlloc = inputBuffers[index]->getAllocation();

					invalidateAlloc(vkd, device, resultAlloc);

					// we always have our result data first
					datas.push_back(resultAlloc.getHostPtr());
				}
			}

			if (!checkResult(internalData, datas, width, subgroupSize, false))
				failIterations++;
			else
				passIterations++;
		}

		vkd.resetCommandBuffer(*cmdBuffer, 0);
	}

	if (failIterations > 0 || passIterations == 0)
		return tcu::TestStatus::fail("Failed " + de::toString(failIterations) + " out of " + de::toString(failIterations + passIterations) + " iterations.");
	else
		return tcu::TestStatus::pass("OK");
}
} // namespace subgroups
} // nsamespace vkt
