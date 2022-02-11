/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Wrapper that can construct monolithic pipeline or use
          VK_EXT_graphics_pipeline_library for pipeline construction
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deSharedPtr.hpp"
#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vk
{

static const VkVertexInputBindingDescription defaultVertexInputBindingDescription
{
	0u,																// deUint32										binding
	sizeof(tcu::Vec4),												// deUint32										stride
	VK_VERTEX_INPUT_RATE_VERTEX,									// VkVertexInputRate							inputRate
};

static const VkVertexInputAttributeDescription defaultVertexInputAttributeDescription
{
	0u,																// deUint32										location
	0u,																// deUint32										binding
	VK_FORMAT_R32G32B32A32_SFLOAT,									// VkFormat										format
	0u																// deUint32										offset
};

static const VkPipelineVertexInputStateCreateInfo defaultVertexInputState
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType
	DE_NULL,														// const void*									pNext
	(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags		flags
	1u,																// deUint32										vertexBindingDescriptionCount
	&defaultVertexInputBindingDescription,							// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
	1u,																// deUint32										vertexAttributeDescriptionCount
	&defaultVertexInputAttributeDescription							// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
};

static const VkStencilOpState defaultStencilOpState
{
	VK_STENCIL_OP_KEEP,												// VkStencilOp									failOp
	VK_STENCIL_OP_KEEP,												// VkStencilOp									passOp
	VK_STENCIL_OP_KEEP,												// VkStencilOp									depthFailOp
	VK_COMPARE_OP_NEVER,											// VkCompareOp									compareOp
	0u,																// deUint32										compareMask
	0u,																// deUint32										writeMask
	0u																// deUint32										reference
};

static const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState
{
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType								sType
	DE_NULL,														// const void*									pNext
	0u,																// VkPipelineDepthStencilStateCreateFlags		flags
	VK_FALSE,														// VkBool32										depthTestEnable
	VK_FALSE,														// VkBool32										depthWriteEnable
	VK_COMPARE_OP_LESS_OR_EQUAL,									// VkCompareOp									depthCompareOp
	VK_FALSE,														// VkBool32										depthBoundsTestEnable
	VK_FALSE,														// VkBool32										stencilTestEnable
	defaultStencilOpState,											// VkStencilOpState								front
	defaultStencilOpState,											// VkStencilOpState								back
	0.0f,															// float										minDepthBounds
	1.0f,															// float										maxDepthBounds
};

static const VkPipelineMultisampleStateCreateInfo defaultMultisampleState
{
	VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType								sType
	DE_NULL,														// const void*									pNext
	0u,																// VkPipelineMultisampleStateCreateFlags		flags
	VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits						rasterizationSamples
	VK_FALSE,														// VkBool32										sampleShadingEnable
	1.0f,															// float										minSampleShading
	DE_NULL,														// const VkSampleMask*							pSampleMask
	VK_FALSE,														// VkBool32										alphaToCoverageEnable
	VK_FALSE														// VkBool32										alphaToOneEnable
};

static const VkPipelineColorBlendAttachmentState defaultColorBlendAttachmentState
{
	VK_FALSE,														// VkBool32										blendEnable
	VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								srcColorBlendFactor
	VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								dstColorBlendFactor
	VK_BLEND_OP_ADD,												// VkBlendOp									colorBlendOp
	VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								srcAlphaBlendFactor
	VK_BLEND_FACTOR_ZERO,											// VkBlendFactor								dstAlphaBlendFactor
	VK_BLEND_OP_ADD,												// VkBlendOp									alphaBlendOp
	0xf																// VkColorComponentFlags						colorWriteMask
};

static const VkPipelineColorBlendStateCreateInfo defaultColorBlendState
{
	VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType
	DE_NULL,														// const void*									pNext
	0u,																// VkPipelineColorBlendStateCreateFlags			flags
	VK_FALSE,														// VkBool32										logicOpEnable
	VK_LOGIC_OP_CLEAR,												// VkLogicOp									logicOp
	1u,																// deUint32										attachmentCount
	&defaultColorBlendAttachmentState,								// const VkPipelineColorBlendAttachmentState*	pAttachments
	{ 0.0f, 0.0f, 0.0f, 0.0f }										// float										blendConstants[4]
};

VkGraphicsPipelineLibraryCreateInfoEXT makeGraphicsPipelineLibraryCreateInfo(const VkGraphicsPipelineLibraryFlagsEXT flags)
{
	return
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,	// VkStructureType						sType;
		DE_NULL,														// void*								pNext;
		flags,															// VkGraphicsPipelineLibraryFlagsEXT	flags;
	};
}

void checkPipelineLibraryRequirements (const InstanceInterface&		vki,
									   VkPhysicalDevice				physicalDevice,
									   PipelineConstructionType		pipelineConstructionType)
{
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		return;

	const auto supportedExtensions = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_EXT_graphics_pipeline_library")))
		TCU_THROW(NotSupportedError, "VK_EXT_graphics_pipeline_library not supported");

	// VK_KHR_pipeline_library must be supported if the VK_EXT_graphics_pipeline_library extension is supported
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_pipeline_library")))
		TCU_FAIL("VK_KHR_pipeline_library not supported but VK_EXT_graphics_pipeline_library supported");

	const vk::VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT pipelineLibraryProperties = getPhysicalDeviceExtensionProperties(vki, physicalDevice);
	if ((pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY) && !pipelineLibraryProperties.graphicsPipelineLibraryFastLinking)
		TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFastLinking is not supported");
}

template<typename StructNext>
void addToChain(void** structThatStartsChain, StructNext* structToAddAtTheEnd)
{
	DE_ASSERT(structThatStartsChain);

	if (structToAddAtTheEnd == DE_NULL)
		return;

	// make sure that pNext pointer of structure that is added to chain is empty;
	// we are construting chains on our own and there are cases that use same
	// structure for multiple instances of GraphicsPipelineWrapper
	structToAddAtTheEnd->pNext = DE_NULL;

	deUint32	safetyCouter	= 10u;
	void**		structInChain	= structThatStartsChain;

	do
	{
		// check if this is free spot
		if (*structInChain == DE_NULL)
		{
			// attach new structure at the end
			*structInChain = structToAddAtTheEnd;
			return;
		}

		// just cast to randomly picked structure - we are only interested in pNext that is second attribute in all structures
		auto* gpl = reinterpret_cast<VkGraphicsPipelineLibraryCreateInfoEXT*>(*structInChain);

		// move structure pointer one position down the pNext chain
		structInChain = &gpl->pNext;
	}
	while (--safetyCouter);

	// probably safetyCouter is to small
	DE_ASSERT(false);
}

// Structure storing *CreateInfo structures that do not need to exist in memory after pipeline was constructed.
struct GraphicsPipelineWrapper::InternalData
{
	const DeviceInterface&								vk;
	VkDevice											device;
	const PipelineConstructionType						pipelineConstructionType;
	const VkPipelineCreateFlags							pipelineFlags;

	// attribute used for making sure pipeline is configured in correct order
	VkGraphicsPipelineLibraryFlagsEXT					setupStates;

	std::vector<VkPipelineShaderStageCreateInfo>		pipelineShaderStages;
	VkPipelineInputAssemblyStateCreateInfo				inputAssemblyState;
	VkPipelineRasterizationStateCreateInfo				defaultRasterizationState;
	VkPipelineViewportStateCreateInfo					viewportState;
	VkPipelineTessellationStateCreateInfo				tessellationState;
	VkPipelineFragmentShadingRateStateCreateInfoKHR*	pFragmentShadingRateState;
	//VkPipelineRenderingCreateInfoKHR*					pRenderingState;
	const VkPipelineDynamicStateCreateInfo*				pDynamicState;

	deBool												useViewportState;
	deBool												useDefaultRasterizationState;
	deBool												useDefaultDepthStencilState;
	deBool												useDefaultColorBlendState;
	deBool												useDefaultMultisampleState;

	VkGraphicsPipelineCreateInfo						monolithicPipelineCreateInfo;

	// initialize with most common values
	InternalData(const DeviceInterface& vkd, VkDevice vkDevice, const PipelineConstructionType constructionType, const VkPipelineCreateFlags pipelineCreateFlags)
		: vk						(vkd)
		, device					(vkDevice)
		, pipelineConstructionType	(constructionType)
		, pipelineFlags				(pipelineCreateFlags)
		, setupStates				(0u)
		, inputAssemblyState
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineInputAssemblyStateCreateFlags		flags
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology							topology
			VK_FALSE														// VkBool32										primitiveRestartEnable
		}
		, defaultRasterizationState
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineRasterizationStateCreateFlags		flags
			VK_FALSE,														// VkBool32										depthClampEnable
			VK_FALSE,														// VkBool32										rasterizerDiscardEnable
			VK_POLYGON_MODE_FILL,											// VkPolygonMode								polygonMode
			VK_CULL_MODE_NONE,												// VkCullModeFlags								cullMode
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace									frontFace
			VK_FALSE,														// VkBool32										depthBiasEnable
			0.0f,															// float										depthBiasConstantFactor
			0.0f,															// float										depthBiasClamp
			0.0f,															// float										depthBiasSlopeFactor
			1.0f															// float										lineWidth
		}
		, viewportState
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags			flags
			1u,																// deUint32										viewportCount
			DE_NULL,														// const VkViewport*							pViewports
			1u,																// deUint32										scissorCount
			DE_NULL															// const VkRect2D*								pScissors
		}
		, tessellationState
		{
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineTessellationStateCreateFlags		flags
			3u																// deUint32										patchControlPoints
		}
		, pFragmentShadingRateState		(DE_NULL)
		//, pRenderingState				(DE_NULL)
		, pDynamicState					(DE_NULL)
		, useViewportState				(DE_TRUE)
		, useDefaultRasterizationState	(DE_FALSE)
		, useDefaultDepthStencilState	(DE_FALSE)
		, useDefaultColorBlendState		(DE_FALSE)
		, useDefaultMultisampleState	(DE_FALSE)
	{
		monolithicPipelineCreateInfo = initVulkanStructure();
	}
};

GraphicsPipelineWrapper::GraphicsPipelineWrapper(const DeviceInterface&				vk,
												 VkDevice							device,
												 const PipelineConstructionType		pipelineConstructionType,
												 const VkPipelineCreateFlags		flags)
	: m_internalData	(new InternalData(vk, device, pipelineConstructionType, flags))
{
}

GraphicsPipelineWrapper::GraphicsPipelineWrapper(GraphicsPipelineWrapper&& pw) noexcept
	: m_pipelineFinal	(pw.m_pipelineFinal)
	, m_internalData	(pw.m_internalData)
{
	std::move(pw.m_pipelineParts, pw.m_pipelineParts + 4, m_pipelineParts);
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setMonolithicPipelineLayout(const VkPipelineLayout layout)
{
	// make sure pipeline was not already built
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	m_internalData->monolithicPipelineCreateInfo.layout = layout;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDynamicState(const VkPipelineDynamicStateCreateInfo* dynamicState)
{
	// make sure states are not yet setup - all pipeline states must know about dynamic state
	DE_ASSERT(m_internalData && m_internalData->setupStates == 0u);

	m_internalData->pDynamicState								= dynamicState;
	m_internalData->monolithicPipelineCreateInfo.pDynamicState	= dynamicState;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultTopology(const VkPrimitiveTopology topology)
{
	// topology is needed by vertex input state, make sure vertex input state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates == 0u));

	m_internalData->inputAssemblyState.topology = topology;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultPatchControlPoints(const deUint32 patchControlPoints)
{
	// patchControlPoints are needed by pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->tessellationState.patchControlPoints = patchControlPoints;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultRasterizerDiscardEnable(const deBool rasterizerDiscardEnable)
{
	// rasterizerDiscardEnable is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->defaultRasterizationState.rasterizerDiscardEnable = rasterizerDiscardEnable;

	return *this;
}


GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultRasterizationState()
{
	// RasterizationState is used in pre-rasterization shader state, make sure this state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->useDefaultRasterizationState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultDepthStencilState()
{
	// DepthStencilState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT));

	m_internalData->useDefaultDepthStencilState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultColorBlendState()
{
	// ColorBlendState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT));

	m_internalData->useDefaultColorBlendState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultMultisampleState()
{
	// MultisampleState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT));

	m_internalData->useDefaultMultisampleState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultViewportsCount(deUint32 viewportCount)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->viewportState.viewportCount = viewportCount;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultScissorsCount(deUint32 scissorCount)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->viewportState.scissorCount = scissorCount;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDepthClipControl(const VkPipelineViewportDepthClipControlCreateInfoEXT* depthClipControlCreateInfo)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->viewportState.pNext = depthClipControlCreateInfo;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::disableViewportState()
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupStates < VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT));

	m_internalData->useViewportState = DE_FALSE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupVertexInputStete(const VkPipelineVertexInputStateCreateInfo*		vertexInputState,
																		const VkPipelineInputAssemblyStateCreateInfo*	inputAssemblyState,
																		const VkPipelineCache							partPipelineCache,
																		VkPipelineCreationFeedbackCreateInfoEXT*		partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set first
	DE_ASSERT(m_internalData && (m_internalData->setupStates == 0u));
	m_internalData->setupStates = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

	const auto pVertexInputState	= vertexInputState ? vertexInputState : &defaultVertexInputState;
	const auto pInputAssemblyState	= inputAssemblyState ? inputAssemblyState : &m_internalData->inputAssemblyState;

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pVertexInputState		= pVertexInputState;
		m_internalData->monolithicPipelineCreateInfo.pInputAssemblyState	= pInputAssemblyState;
	}
	else
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, partCreationFeedback);

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelinePartCreateInfo.pVertexInputState	= pVertexInputState;
		pipelinePartCreateInfo.pInputAssemblyState	= pInputAssemblyState;
		pipelinePartCreateInfo.pDynamicState		= m_internalData->pDynamicState;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[0] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupPreRasterizationShaderState(const std::vector<VkViewport>&					viewports,
																				   const std::vector<VkRect2D>&						scissors,
																				   const VkPipelineLayout							layout,
																				   const VkRenderPass								renderPass,
																				   const deUint32									subpass,
																				   const VkShaderModule								vertexShaderModule,
																				   const VkPipelineRasterizationStateCreateInfo*	rasterizationState,
																				   const VkShaderModule								tessellationControlShaderModule,
																				   const VkShaderModule								tessellationEvalShaderModule,
																				   const VkShaderModule								geometryShaderModule,
																				   const VkSpecializationInfo*						specializationInfo,
																				   /*VkPipelineRenderingCreateInfoKHR*				rendering,*/
																				   const VkPipelineCache							partPipelineCache,
																				   VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set second
	DE_ASSERT(m_internalData && (m_internalData->setupStates == VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT));
	m_internalData->setupStates |= VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

	//m_internalData->pRenderingState = rendering;

	const bool hasTesc = (tessellationControlShaderModule != DE_NULL);
	const bool hasTese = (tessellationEvalShaderModule != DE_NULL);
	const bool hasGeom = (geometryShaderModule != DE_NULL);

	const auto pRasterizationState = rasterizationState ? rasterizationState
														: (m_internalData->useDefaultRasterizationState ? &m_internalData->defaultRasterizationState : DE_NULL);
	const auto pTessellationState	= (hasTesc || hasTese) ? &m_internalData->tessellationState : DE_NULL;
	const auto pViewportState		= m_internalData->useViewportState ? &m_internalData->viewportState : DE_NULL;

	// reserve space for all stages including fragment - this is needed when we create monolithic pipeline
	m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(2u + hasTesc + hasTese + hasGeom,
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
		DE_NULL,												// const void*							pNext
		0u,														// VkPipelineShaderStageCreateFlags		flags
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage
		vertexShaderModule,										// VkShaderModule						module
		"main",													// const char*							pName
		specializationInfo										// const VkSpecializationInfo*			pSpecializationInfo
	});

	std::vector<VkPipelineShaderStageCreateInfo>::iterator currStage = m_internalData->pipelineShaderStages.begin() + 1;
	if (hasTesc)
	{
		currStage->stage	= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		currStage->module	= tessellationControlShaderModule;
		++currStage;
	}
	if (hasTese)
	{
		currStage->stage	= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		currStage->module	= tessellationEvalShaderModule;
		++currStage;
	}
	if (hasGeom)
	{
		currStage->stage	= VK_SHADER_STAGE_GEOMETRY_BIT;
		currStage->module	= geometryShaderModule;
	}

	if (pViewportState)
	{
		if (!viewports.empty())
		{
			pViewportState->viewportCount	= (deUint32)viewports.size();
			pViewportState->pViewports		= &viewports[0];
		}
		if (!scissors.empty())
		{
			pViewportState->scissorCount	= (deUint32)scissors.size();
			pViewportState->pScissors		= &scissors[0];
		}
	}

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		// make sure we dont overwrite layout specified with setupMonolithicPipelineLayout
		if (m_internalData->monolithicPipelineCreateInfo.layout == 0)
			m_internalData->monolithicPipelineCreateInfo.layout = layout;

		m_internalData->monolithicPipelineCreateInfo.renderPass				= renderPass;
		m_internalData->monolithicPipelineCreateInfo.subpass				= subpass;
		m_internalData->monolithicPipelineCreateInfo.pRasterizationState	= pRasterizationState;
		m_internalData->monolithicPipelineCreateInfo.pViewportState			= pViewportState;
		m_internalData->monolithicPipelineCreateInfo.stageCount				= 1u + hasTesc + hasTese + hasGeom;
		m_internalData->monolithicPipelineCreateInfo.pStages				= m_internalData->pipelineShaderStages.data();
		m_internalData->monolithicPipelineCreateInfo.pTessellationState		= pTessellationState;
	}
	else
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		//addToChain(&firstStructInChain, m_internalData->pRenderingState);
		addToChain(&firstStructInChain, partCreationFeedback);

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelinePartCreateInfo.layout				= layout;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pRasterizationState	= pRasterizationState;
		pipelinePartCreateInfo.pViewportState		= pViewportState;
		pipelinePartCreateInfo.stageCount			= 1u + hasTesc + hasTese + hasGeom;
		pipelinePartCreateInfo.pStages				= m_internalData->pipelineShaderStages.data();
		pipelinePartCreateInfo.pTessellationState	= pTessellationState;
		pipelinePartCreateInfo.pDynamicState		= m_internalData->pDynamicState;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[1] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupFragmentShaderState(const VkPipelineLayout								layout,
																		   const VkRenderPass									renderPass,
																		   const deUint32										subpass,
																		   const VkShaderModule									fragmentShaderModule,
																		   const VkPipelineDepthStencilStateCreateInfo*			depthStencilState,
																		   const VkPipelineMultisampleStateCreateInfo*			multisampleState,
																		   VkPipelineFragmentShadingRateStateCreateInfoKHR*		fragmentShadingRateState,
																		   const VkSpecializationInfo*							specializationInfo,
																		   const VkPipelineCache								partPipelineCache,
																		   VkPipelineCreationFeedbackCreateInfoEXT*				partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set third
	DE_ASSERT(m_internalData && (m_internalData->setupStates == (VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)));
	m_internalData->setupStates |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

	m_internalData->pFragmentShadingRateState = fragmentShadingRateState;

	const auto pDepthStencilState	= depthStencilState ? depthStencilState
														: (m_internalData->useDefaultDepthStencilState ? &defaultDepthStencilState : DE_NULL);
	const auto pMultisampleState	= multisampleState ? multisampleState
														: (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);
	const bool hasFrag				= (fragmentShaderModule != DE_NULL);

	deUint32 stageIndex = 1;
	if (hasFrag)
	{
		// find free space for fragment shader
		for (; stageIndex < 5; ++stageIndex)
		{
			if (m_internalData->pipelineShaderStages[stageIndex].stage == VK_SHADER_STAGE_VERTEX_BIT)
			{
				m_internalData->pipelineShaderStages[stageIndex].stage					= VK_SHADER_STAGE_FRAGMENT_BIT;
				m_internalData->pipelineShaderStages[stageIndex].module					= fragmentShaderModule;
				m_internalData->pipelineShaderStages[stageIndex].pSpecializationInfo	= specializationInfo;
				break;
			}
		}
	}

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pDepthStencilState	= pDepthStencilState;
		m_internalData->monolithicPipelineCreateInfo.pMultisampleState	= pMultisampleState;
		m_internalData->monolithicPipelineCreateInfo.stageCount			+= !!fragmentShaderModule;
	}
	else
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
		//addToChain(&firstStructInChain, m_internalData->pRenderingState);
		addToChain(&firstStructInChain, partCreationFeedback);

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelinePartCreateInfo.layout				= layout;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pDepthStencilState	= pDepthStencilState;
		pipelinePartCreateInfo.pMultisampleState	= pMultisampleState;
		pipelinePartCreateInfo.stageCount			= hasFrag;
		pipelinePartCreateInfo.pStages				= hasFrag ? &m_internalData->pipelineShaderStages[stageIndex] : DE_NULL;
		pipelinePartCreateInfo.pDynamicState		= m_internalData->pDynamicState;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[2] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupFragmentOutputState(const VkRenderPass								renderPass,
																		   const deUint32									subpass,
																		   const VkPipelineColorBlendStateCreateInfo*		colorBlendState,
																		   const VkPipelineMultisampleStateCreateInfo*		multisampleState,
																		   //const VkPipelineRenderingCreateInfoKHR*		rendering,
																		   const VkPipelineCache							partPipelineCache,
																		   VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set last
	DE_ASSERT(m_internalData && (m_internalData->setupStates == (VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)));
	m_internalData->setupStates |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

	const auto pColorBlendState		= colorBlendState ? colorBlendState
														: (m_internalData->useDefaultColorBlendState ? &defaultColorBlendState : DE_NULL);
	const auto pMultisampleState	= multisampleState ? multisampleState
														: (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);


	void* firstStructInChain = DE_NULL;
	addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
	//addToChain(&firstStructInChain, m_internalData->pRenderingState);

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pNext				= firstStructInChain;
		m_internalData->monolithicPipelineCreateInfo.flags				= m_internalData->pipelineFlags;
		m_internalData->monolithicPipelineCreateInfo.pColorBlendState	= pColorBlendState;
		m_internalData->monolithicPipelineCreateInfo.pMultisampleState	= pMultisampleState;
	}
	else
	{
		auto libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
		addToChain(&firstStructInChain, &libraryCreateInfo);
		addToChain(&firstStructInChain, partCreationFeedback);

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pColorBlendState		= pColorBlendState;
		pipelinePartCreateInfo.pMultisampleState	= pMultisampleState;
		pipelinePartCreateInfo.pDynamicState		= m_internalData->pDynamicState;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[3] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}

	return *this;
}

void GraphicsPipelineWrapper::buildPipeline(const VkPipelineCache						pipelineCache,
											const VkPipeline							basePipelineHandle,
											const deInt32								basePipelineIndex,
											VkPipelineCreationFeedbackCreateInfoEXT*	creationFeedback)
{
	// make sure we are not trying to build pipeline second time
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure all states were set
	DE_ASSERT(m_internalData && (m_internalData->setupStates == (VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
																 VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)));

	VkGraphicsPipelineCreateInfo*	pointerToCreateInfo	= &m_internalData->monolithicPipelineCreateInfo;
	VkGraphicsPipelineCreateInfo	linkedCreateInfo	= initVulkanStructure();
	VkPipeline						rawPipelines[4];
	VkPipelineLibraryCreateInfoKHR	linkingInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,		// VkStructureType		sType;
		creationFeedback,										// const void*			pNext;
		4,														// deUint32				libraryCount;
		rawPipelines											// const VkPipeline*	pLibraries;
	};

	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		for (deUint32 i = 0; i < 4; ++i)
			rawPipelines[i] = m_pipelineParts[i].get();

		linkedCreateInfo.pNext	= &linkingInfo;
		linkedCreateInfo.flags	= m_internalData->pipelineFlags;
		pointerToCreateInfo		= &linkedCreateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			linkedCreateInfo.flags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
	}
	else
	{
		// note: there might be other structures in the chain already
		void* firstStructInChain = static_cast<void*>(pointerToCreateInfo);
		addToChain(&firstStructInChain, creationFeedback);
	}

	pointerToCreateInfo->basePipelineHandle	= basePipelineHandle;
	pointerToCreateInfo->basePipelineIndex	= basePipelineIndex;

	m_pipelineFinal = createGraphicsPipeline(m_internalData->vk, m_internalData->device, pipelineCache, pointerToCreateInfo);

	// pipeline was created - we can free CreateInfo structures
	m_internalData.clear();
}

deBool GraphicsPipelineWrapper::wasBuild() const
{
	return !!m_pipelineFinal.get();
}

VkPipeline GraphicsPipelineWrapper::getPipeline() const
{
	DE_ASSERT(m_pipelineFinal.get() != DE_NULL);
	return m_pipelineFinal.get();
}

void GraphicsPipelineWrapper::destroyPipeline(void)
{
	DE_ASSERT(m_pipelineFinal.get() != DE_NULL);

	m_pipelineFinal = Move<VkPipeline>();
}

} // vk
