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
#include "deSTLUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "tcuMaybe.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include <memory>
#include <set>

namespace vk
{

namespace
{

enum PipelineSetupState
{
	PSS_NONE						= 0x00000000,
	PSS_VERTEX_INPUT_INTERFACE		= 0x00000001,
	PSS_PRE_RASTERIZATION_SHADERS	= 0x00000002,
	PSS_FRAGMENT_SHADER				= 0x00000004,
	PSS_FRAGMENT_OUTPUT_INTERFACE	= 0x00000008,
};

using TessellationDomainOriginStatePtr = std::unique_ptr<VkPipelineTessellationDomainOriginStateCreateInfo>;

} // anonymous namespace

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

namespace
{
#ifndef CTS_USES_VULKANSC
VkGraphicsPipelineLibraryCreateInfoEXT makeGraphicsPipelineLibraryCreateInfo(const VkGraphicsPipelineLibraryFlagsEXT flags)
{
	return
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,	// VkStructureType						sType;
		DE_NULL,														// void*								pNext;
		flags,															// VkGraphicsPipelineLibraryFlagsEXT	flags;
	};
}
#endif // CTS_USES_VULKANSC

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&				vk,
									   VkDevice								device,
									   VkPipelineCache						pipelineCache,
									   const VkGraphicsPipelineCreateInfo*	pCreateInfo,
									   const VkAllocationCallbacks*			pAllocator = nullptr)
{
	VkPipeline	object					= 0;
	const auto	retcode					= vk.createGraphicsPipelines(device, pipelineCache, 1u, pCreateInfo, pAllocator, &object);

#ifndef CTS_USES_VULKANSC
	const bool	allowCompileRequired	= ((pCreateInfo->flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0u);

	if (allowCompileRequired && retcode == VK_PIPELINE_COMPILE_REQUIRED)
		throw PipelineCompileRequiredError("createGraphicsPipelines returned VK_PIPELINE_COMPILE_REQUIRED");
#endif // CTS_USES_VULKANSC

	VK_CHECK(retcode);
	return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

} // anonymous

void checkPipelineLibraryRequirements (const InstanceInterface&		vki,
									   VkPhysicalDevice				physicalDevice,
									   PipelineConstructionType		pipelineConstructionType)
{
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		return;

	const auto supportedExtensions = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
	if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_graphics_pipeline_library")))
		TCU_THROW(NotSupportedError, "VK_EXT_graphics_pipeline_library not supported");
}

void addToChain(void** structThatStartsChain, void* structToAddAtTheEnd)
{
	DE_ASSERT(structThatStartsChain);

	if (structToAddAtTheEnd == DE_NULL)
		return;

	// Cast to the base out structure which has a non-const pNext pointer.
	auto* structToAddAtTheEndCasted = reinterpret_cast<VkBaseOutStructure*>(structToAddAtTheEnd);

	// make sure that pNext pointer of structure that is added to chain is empty;
	// we are construting chains on our own and there are cases that use same
	// structure for multiple instances of GraphicsPipelineWrapper
	structToAddAtTheEndCasted->pNext = DE_NULL;

	deUint32	safetyCouter	= 10u;
	void**		structInChain	= structThatStartsChain;

	do
	{
		// check if this is free spot
		if (*structInChain == DE_NULL)
		{
			// attach new structure at the end
			*structInChain = structToAddAtTheEndCasted;
			return;
		}

		// Cast to the base out structure which has a non-const pNext pointer.
		auto* gpl = reinterpret_cast<VkBaseOutStructure*>(*structInChain);

		// move structure pointer one position down the pNext chain
		structInChain = reinterpret_cast<void**>(&gpl->pNext);
	}
	while (--safetyCouter);

	// probably safetyCouter is to small
	DE_ASSERT(false);
}

namespace {
	using PipelineShaderStageModuleIdPtr = std::unique_ptr<PipelineShaderStageModuleIdentifierCreateInfoWrapper>;
}

// Structure storing *CreateInfo structures that do not need to exist in memory after pipeline was constructed.
struct GraphicsPipelineWrapper::InternalData
{
	const DeviceInterface&								vk;
	VkDevice											device;
	const PipelineConstructionType						pipelineConstructionType;
	const VkPipelineCreateFlags							pipelineFlags;

	// attribute used for making sure pipeline is configured in correct order
	int													setupState;

	std::vector<PipelineShaderStageModuleIdPtr>			pipelineShaderIdentifiers;
	std::vector<VkPipelineShaderStageCreateInfo>		pipelineShaderStages;
	VkPipelineInputAssemblyStateCreateInfo				inputAssemblyState;
	VkPipelineRasterizationStateCreateInfo				defaultRasterizationState;
	VkPipelineViewportStateCreateInfo					viewportState;
	VkPipelineTessellationStateCreateInfo				tessellationState;
	VkPipelineFragmentShadingRateStateCreateInfoKHR*	pFragmentShadingRateState;
	PipelineRenderingCreateInfoWrapper					pRenderingState;
	const VkPipelineDynamicStateCreateInfo*				pDynamicState;
	PipelineRepresentativeFragmentTestCreateInfoWrapper	pRepresentativeFragmentTestState;

	TessellationDomainOriginStatePtr					pTessellationDomainOrigin;
	deBool												useViewportState;
	deBool												useDefaultRasterizationState;
	deBool												useDefaultDepthStencilState;
	deBool												useDefaultColorBlendState;
	deBool												useDefaultMultisampleState;
	bool												failOnCompileWhenLinking;

	VkGraphicsPipelineCreateInfo						monolithicPipelineCreateInfo;

	// initialize with most common values
	InternalData(const DeviceInterface& vkd, VkDevice vkDevice, const PipelineConstructionType constructionType, const VkPipelineCreateFlags pipelineCreateFlags)
		: vk						(vkd)
		, device					(vkDevice)
		, pipelineConstructionType	(constructionType)
		, pipelineFlags				(pipelineCreateFlags)
		, setupState				(PSS_NONE)
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
		, pFragmentShadingRateState		(nullptr)
		, pDynamicState					(DE_NULL)
		, pRepresentativeFragmentTestState(nullptr)
		, pTessellationDomainOrigin		()
		, useViewportState				(DE_TRUE)
		, useDefaultRasterizationState	(DE_FALSE)
		, useDefaultDepthStencilState	(DE_FALSE)
		, useDefaultColorBlendState		(DE_FALSE)
		, useDefaultMultisampleState	(DE_FALSE)
		, failOnCompileWhenLinking		(false)
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
	std::move(pw.m_pipelineParts, pw.m_pipelineParts + de::arrayLength(pw.m_pipelineParts), m_pipelineParts);
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
	DE_ASSERT(m_internalData && m_internalData->setupState == PSS_NONE);

	m_internalData->pDynamicState								= dynamicState;
	m_internalData->monolithicPipelineCreateInfo.pDynamicState	= dynamicState;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setRepresentativeFragmentTestState(PipelineRepresentativeFragmentTestCreateInfoWrapper representativeFragmentTestState)
{
	// Representative fragment test state is needed by the fragment shader state.
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

	m_internalData->pRepresentativeFragmentTestState = representativeFragmentTestState;
	return *this;
}

std::vector<VkDynamicState> getDynamicStates(const VkPipelineDynamicStateCreateInfo* dynamicStateInfo, uint32_t setupState)
{
	static const std::set<VkDynamicState> vertexInputStates {
		VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
		VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
		VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT,
	};

	static const std::set<VkDynamicState> preRastStates {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,
		VK_DYNAMIC_STATE_LINE_WIDTH,
		VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
		VK_DYNAMIC_STATE_CULL_MODE_EXT,
		VK_DYNAMIC_STATE_FRONT_FACE_EXT,
		VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
		VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT,
		VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,
		VK_DYNAMIC_STATE_DEPTH_BIAS,
		VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT,
		VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
#ifndef CTS_USES_VULKANSC
		VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT,
		VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
		VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
		VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT,
		VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT,
		VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT,
		VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT,
		VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT,
		VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
		VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT,
		VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT,
		VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT,
		VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV,
		VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV,
		VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV,
		VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,
		VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,
		VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,
		VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,
#endif
	};

	static const std::set<VkDynamicState> fragShaderStates {
		VK_DYNAMIC_STATE_DEPTH_BOUNDS,
		VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
		VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
		VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
		VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
		VK_DYNAMIC_STATE_STENCIL_OP_EXT,
		VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
		// Needs MSAA info here as well as fragment output state
		VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
#ifndef CTS_USES_VULKANSC
		VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
		VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
		VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
		VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,
		VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
		VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,
		VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,
#endif
	};

	static const std::set<VkDynamicState> fragOutputStates {
		VK_DYNAMIC_STATE_LOGIC_OP_EXT,
		VK_DYNAMIC_STATE_BLEND_CONSTANTS,
		VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
		VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
		VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
#ifndef CTS_USES_VULKANSC
		VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
		VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
		VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT,
		VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
		VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
		VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
		VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
		VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
		VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,
		VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
		VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,
		VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,
		VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,
		VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT,
#endif
	};

	const std::set<VkDynamicState> dynamicStates (dynamicStateInfo->pDynamicStates,
												  dynamicStateInfo->pDynamicStates + dynamicStateInfo->dynamicStateCount);

	// Verify all passed states are contained in at least one of the vectors above, so they won't get lost.
	for (const auto dynState : dynamicStates)
	{
		DE_UNREF(dynState); // For release builds.
		DE_ASSERT(		de::contains(vertexInputStates.begin(),	vertexInputStates.end(),	dynState)
				  ||	de::contains(preRastStates.begin(),		preRastStates.end(),		dynState)
				  ||	de::contains(fragShaderStates.begin(),	fragShaderStates.end(),		dynState)
				  ||	de::contains(fragOutputStates.begin(),	fragOutputStates.end(),		dynState));
	}

	std::set<VkDynamicState> intersectedStates;

	if (setupState & PSS_VERTEX_INPUT_INTERFACE)
		std::set_intersection(vertexInputStates.begin(), vertexInputStates.end(), dynamicStates.begin(), dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

	if (setupState & PSS_PRE_RASTERIZATION_SHADERS)
		std::set_intersection(preRastStates.begin(),	 preRastStates.end(),	  dynamicStates.begin(), dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

	if (setupState & PSS_FRAGMENT_SHADER)
		std::set_intersection(fragShaderStates.begin(),  fragShaderStates.end(),  dynamicStates.begin(), dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

	if (setupState & PSS_FRAGMENT_OUTPUT_INTERFACE)
		std::set_intersection(fragOutputStates.begin(),  fragOutputStates.end(),  dynamicStates.begin(), dynamicStates.end(), std::inserter(intersectedStates, intersectedStates.end()));

	const std::vector<VkDynamicState> returnedStates (begin(intersectedStates), end(intersectedStates));

	return returnedStates;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultTopology(const VkPrimitiveTopology topology)
{
	// topology is needed by vertex input state, make sure vertex input state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

	m_internalData->inputAssemblyState.topology = topology;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultPatchControlPoints(const deUint32 patchControlPoints)
{
	// patchControlPoints are needed by pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->tessellationState.patchControlPoints = patchControlPoints;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultTessellationDomainOrigin (const VkTessellationDomainOrigin domainOrigin, bool forceExtStruct)
{
	// Tessellation domain origin is needed by pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	// We need the extension structure when:
	// - We want to force it.
	// - The domain origin is not the default value.
	// - We have already hooked the extension structure.
	if (forceExtStruct || domainOrigin != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT || m_internalData->pTessellationDomainOrigin)
	{
		if (!m_internalData->pTessellationDomainOrigin)
		{
			m_internalData->pTessellationDomainOrigin.reset(new VkPipelineTessellationDomainOriginStateCreateInfo(initVulkanStructure()));
			m_internalData->tessellationState.pNext = m_internalData->pTessellationDomainOrigin.get();
		}
		m_internalData->pTessellationDomainOrigin->domainOrigin = domainOrigin;
	}

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultRasterizerDiscardEnable(const deBool rasterizerDiscardEnable)
{
	// rasterizerDiscardEnable is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->defaultRasterizationState.rasterizerDiscardEnable = rasterizerDiscardEnable;

	return *this;
}


GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultRasterizationState()
{
	// RasterizationState is used in pre-rasterization shader state, make sure this state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->useDefaultRasterizationState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultDepthStencilState()
{
	// DepthStencilState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

	m_internalData->useDefaultDepthStencilState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultColorBlendState()
{
	// ColorBlendState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

	m_internalData->useDefaultColorBlendState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultMultisampleState()
{
	// MultisampleState is used in fragment shader state, make sure fragment shader state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_FRAGMENT_SHADER));

	m_internalData->useDefaultMultisampleState = DE_TRUE;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultViewportsCount(deUint32 viewportCount)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->viewportState.viewportCount = viewportCount;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setDefaultScissorsCount(deUint32 scissorCount)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->viewportState.scissorCount = scissorCount;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setViewportStatePnext(const void* pNext)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->viewportState.pNext = pNext;

	return *this;
}

#ifndef CTS_USES_VULKANSC
GraphicsPipelineWrapper& GraphicsPipelineWrapper::setRenderingColorAttachmentsInfo(PipelineRenderingCreateInfoWrapper pipelineRenderingCreateInfo)
{
	/* When both graphics pipeline library and dynamic rendering enabled, we just need only viewMask of VkPipelineRenderingCreateInfo
	 * on non-fragment stages. But we need the rest info for setting up fragment output states.
	 * This method provides a way to verify this condition.
	 */
	if (!m_internalData->pRenderingState.ptr || m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		return *this;

	DE_ASSERT(m_internalData && (m_internalData->setupState > PSS_VERTEX_INPUT_INTERFACE) &&
								(m_internalData->setupState < PSS_FRAGMENT_OUTPUT_INTERFACE) &&
								(m_internalData->pRenderingState.ptr->viewMask == pipelineRenderingCreateInfo.ptr->viewMask));

	m_internalData->pRenderingState.ptr = pipelineRenderingCreateInfo.ptr;

	return *this;
}
#endif

GraphicsPipelineWrapper& GraphicsPipelineWrapper::disableViewportState(const bool disable)
{
	// ViewportState is used in pre-rasterization shader state, make sure pre-rasterization state was not setup yet
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	m_internalData->useViewportState = !disable;

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupVertexInputState(const VkPipelineVertexInputStateCreateInfo*		vertexInputState,
																		const VkPipelineInputAssemblyStateCreateInfo*	inputAssemblyState,
																		const VkPipelineCache							partPipelineCache,
																		PipelineCreationFeedbackCreateInfoWrapper		partCreationFeedback,
																		const bool										useNullPtrs)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set first
	DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_NONE));

	// Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
	DE_UNREF(partPipelineCache);
	DE_UNREF(partCreationFeedback);

	m_internalData->setupState = PSS_VERTEX_INPUT_INTERFACE;

	const auto pVertexInputState = ((vertexInputState || useNullPtrs) ? vertexInputState : &defaultVertexInputState);
	const auto pInputAssemblyState = ((inputAssemblyState || useNullPtrs) ? inputAssemblyState : &m_internalData->inputAssemblyState);

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pVertexInputState = pVertexInputState;
		m_internalData->monolithicPipelineCreateInfo.pInputAssemblyState = pInputAssemblyState;
	}

#ifndef CTS_USES_VULKANSC
	// note we could just use else to if statement above but sinc
	// this section is cut out for Vulkan SC its cleaner with separate if
	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		auto	libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
		void*	firstStructInChain = reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, partCreationFeedback.ptr);

		VkPipelineDynamicStateCreateInfo pickedDynamicStateInfo = initVulkanStructure();
		std::vector<VkDynamicState> states;

		if(m_internalData->pDynamicState)
		{
			states = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

			pickedDynamicStateInfo.pDynamicStates = states.data();
			pickedDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
		}

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext						= firstStructInChain;
		pipelinePartCreateInfo.flags						= (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelinePartCreateInfo.pVertexInputState			= pVertexInputState;
		pipelinePartCreateInfo.pInputAssemblyState			= pInputAssemblyState;
		pipelinePartCreateInfo.pDynamicState				= &pickedDynamicStateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[0] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}
#endif // CTS_USES_VULKANSC

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
																				   const VkSpecializationInfo						*specializationInfo,
																				   VkPipelineFragmentShadingRateStateCreateInfoKHR*	fragmentShadingRateState,
																				   PipelineRenderingCreateInfoWrapper				rendering,
																				   const VkPipelineCache							partPipelineCache,
																				   PipelineCreationFeedbackCreateInfoWrapper		partCreationFeedback)
{
	return setupPreRasterizationShaderState2(viewports,
											 scissors,
											 layout,
											 renderPass,
											 subpass,
											 vertexShaderModule,
											 rasterizationState,
											 tessellationControlShaderModule,
											 tessellationEvalShaderModule,
											 geometryShaderModule,
											 // Reuse the same specialization info for all stages.
											 specializationInfo,
											 specializationInfo,
											 specializationInfo,
											 specializationInfo,
											 fragmentShadingRateState,
											 rendering,
											 partPipelineCache,
											 partCreationFeedback);
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupPreRasterizationShaderState2(const std::vector<VkViewport>&					viewports,
																					const std::vector<VkRect2D>&					scissors,
																					const VkPipelineLayout							layout,
																					const VkRenderPass								renderPass,
																					const deUint32									subpass,
																					const VkShaderModule							vertexShaderModule,
																					const VkPipelineRasterizationStateCreateInfo*	rasterizationState,
																					const VkShaderModule							tessellationControlShaderModule,
																					const VkShaderModule							tessellationEvalShaderModule,
																					const VkShaderModule							geometryShaderModule,
																					const VkSpecializationInfo*						vertSpecializationInfo,
																					const VkSpecializationInfo*						tescSpecializationInfo,
																					const VkSpecializationInfo*						teseSpecializationInfo,
																					const VkSpecializationInfo*						geomSpecializationInfo,
																					VkPipelineFragmentShadingRateStateCreateInfoKHR*fragmentShadingRateState,
																					PipelineRenderingCreateInfoWrapper				rendering,
																					const VkPipelineCache							partPipelineCache,
																					PipelineCreationFeedbackCreateInfoWrapper		partCreationFeedback)
{
	return setupPreRasterizationShaderState3(viewports,
											 scissors,
											 layout,
											 renderPass,
											 subpass,
											 vertexShaderModule,
											 PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
											 rasterizationState,
											 tessellationControlShaderModule,
											 PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
											 tessellationEvalShaderModule,
											 PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
											 geometryShaderModule,
											 PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
											 vertSpecializationInfo,
											 tescSpecializationInfo,
											 teseSpecializationInfo,
											 geomSpecializationInfo,
											 fragmentShadingRateState,
											 rendering,
											 partPipelineCache,
											 partCreationFeedback);
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupPreRasterizationShaderState3(const std::vector<VkViewport>&								viewports,
																					const std::vector<VkRect2D>&								scissors,
																					const VkPipelineLayout										layout,
																					const VkRenderPass											renderPass,
																					const deUint32												subpass,
																					const VkShaderModule										vertexShaderModule,
																					PipelineShaderStageModuleIdentifierCreateInfoWrapper		vertShaderModuleId,
																					const VkPipelineRasterizationStateCreateInfo*				rasterizationState,
																					const VkShaderModule										tessellationControlShaderModule,
																					PipelineShaderStageModuleIdentifierCreateInfoWrapper		tescShaderModuleId,
																					const VkShaderModule										tessellationEvalShaderModule,
																					PipelineShaderStageModuleIdentifierCreateInfoWrapper		teseShaderModuleId,
																					const VkShaderModule										geometryShaderModule,
																					PipelineShaderStageModuleIdentifierCreateInfoWrapper		geomShaderModuleId,
																					const VkSpecializationInfo*									vertSpecializationInfo,
																					const VkSpecializationInfo*									tescSpecializationInfo,
																					const VkSpecializationInfo*									teseSpecializationInfo,
																					const VkSpecializationInfo*									geomSpecializationInfo,
																					VkPipelineFragmentShadingRateStateCreateInfoKHR*			fragmentShadingRateState,
																					PipelineRenderingCreateInfoWrapper							rendering,
																					const VkPipelineCache										partPipelineCache,
																					PipelineCreationFeedbackCreateInfoWrapper					partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set second
	DE_ASSERT(m_internalData && (m_internalData->setupState == PSS_VERTEX_INPUT_INTERFACE));

	// Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
	DE_UNREF(partPipelineCache);
	DE_UNREF(partCreationFeedback);
	DE_UNREF(vertShaderModuleId);
	DE_UNREF(tescShaderModuleId);
	DE_UNREF(teseShaderModuleId);
	DE_UNREF(geomShaderModuleId);

	m_internalData->setupState |= PSS_PRE_RASTERIZATION_SHADERS;
	m_internalData->pFragmentShadingRateState = fragmentShadingRateState;
	m_internalData->pRenderingState.ptr = rendering.ptr;

	const bool hasTesc = (tessellationControlShaderModule != DE_NULL || tescShaderModuleId.ptr);
	const bool hasTese = (tessellationEvalShaderModule != DE_NULL || teseShaderModuleId.ptr);
	const bool hasGeom = (geometryShaderModule != DE_NULL || geomShaderModuleId.ptr);

	const auto pRasterizationState = rasterizationState ? rasterizationState
														: (m_internalData->useDefaultRasterizationState ? &m_internalData->defaultRasterizationState : DE_NULL);
	const bool forceNullTessState	= (m_internalData->tessellationState.patchControlPoints == std::numeric_limits<uint32_t>::max());
	const auto pTessellationState	= ((hasTesc || hasTese) && !forceNullTessState) ? &m_internalData->tessellationState : nullptr;
	const auto pViewportState		= m_internalData->useViewportState ? &m_internalData->viewportState : DE_NULL;

	VkPipelineCreateFlags shaderModuleIdFlags = 0u;

	// reserve space for all stages including fragment - this is needed when we create monolithic pipeline
	m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(2u + hasTesc + hasTese + hasGeom,
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
		DE_NULL,												// const void*							pNext
		0u,														// VkPipelineShaderStageCreateFlags		flags
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage
		vertexShaderModule,										// VkShaderModule						module
		"main",													// const char*							pName
		vertSpecializationInfo									// const VkSpecializationInfo*			pSpecializationInfo
	});

#ifndef CTS_USES_VULKANSC
	if (vertShaderModuleId.ptr)
	{
		m_internalData->pipelineShaderIdentifiers.emplace_back(new PipelineShaderStageModuleIdentifierCreateInfoWrapper(vertShaderModuleId.ptr));
		m_internalData->pipelineShaderStages[0].pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

		if (vertexShaderModule == DE_NULL)
			shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
	}
#endif // CTS_USES_VULKANSC

	std::vector<VkPipelineShaderStageCreateInfo>::iterator currStage = m_internalData->pipelineShaderStages.begin() + 1;

	if (hasTesc)
	{
		currStage->stage				= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		currStage->module				= tessellationControlShaderModule;
		currStage->pSpecializationInfo	= tescSpecializationInfo;

#ifndef CTS_USES_VULKANSC
		if (tescShaderModuleId.ptr)
		{
			m_internalData->pipelineShaderIdentifiers.emplace_back(new PipelineShaderStageModuleIdentifierCreateInfoWrapper(tescShaderModuleId.ptr));
			currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

			if (tessellationControlShaderModule == DE_NULL)
				shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
		}
#endif // CTS_USES_VULKANSC

		++currStage;
	}

	if (hasTese)
	{
		currStage->stage				= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		currStage->module				= tessellationEvalShaderModule;
		currStage->pSpecializationInfo	= teseSpecializationInfo;

#ifndef CTS_USES_VULKANSC
		if (teseShaderModuleId.ptr)
		{
			m_internalData->pipelineShaderIdentifiers.emplace_back(new PipelineShaderStageModuleIdentifierCreateInfoWrapper(teseShaderModuleId.ptr));
			currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

			if (tessellationEvalShaderModule == DE_NULL)
				shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
		}
#endif // CTS_USES_VULKANSC

		++currStage;
	}

	if (hasGeom)
	{
		currStage->stage				= VK_SHADER_STAGE_GEOMETRY_BIT;
		currStage->module				= geometryShaderModule;
		currStage->pSpecializationInfo	= geomSpecializationInfo;

#ifndef CTS_USES_VULKANSC
		if (geomShaderModuleId.ptr)
		{
			m_internalData->pipelineShaderIdentifiers.emplace_back(new PipelineShaderStageModuleIdentifierCreateInfoWrapper(geomShaderModuleId.ptr));
			currStage->pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

			if (geometryShaderModule == DE_NULL)
				shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
		}
#endif // CTS_USES_VULKANSC
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
		m_internalData->monolithicPipelineCreateInfo.flags					|= shaderModuleIdFlags;
	}

#ifndef CTS_USES_VULKANSC
	// note we could just use else to if statement above but sinc
	// this section is cut out for Vulkan SC its cleaner with separate if
	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
		addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
		addToChain(&firstStructInChain, partCreationFeedback.ptr);

		VkPipelineDynamicStateCreateInfo pickedDynamicStateInfo = initVulkanStructure();
		std::vector<VkDynamicState> states;

		if(m_internalData->pDynamicState)
		{
			states = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

			pickedDynamicStateInfo.pDynamicStates = states.data();
			pickedDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
		}

		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelinePartCreateInfo.layout				= layout;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pRasterizationState	= pRasterizationState;
		pipelinePartCreateInfo.pViewportState		= pViewportState;
		pipelinePartCreateInfo.stageCount			= 1u + hasTesc + hasTese + hasGeom;
		pipelinePartCreateInfo.pStages				= m_internalData->pipelineShaderStages.data();
		pipelinePartCreateInfo.pTessellationState	= pTessellationState;
		pipelinePartCreateInfo.pDynamicState		= &pickedDynamicStateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
			m_internalData->failOnCompileWhenLinking = true;

		m_pipelineParts[1] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}
#endif // CTS_USES_VULKANSC

	return *this;
}

#ifndef CTS_USES_VULKANSC
GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupPreRasterizationMeshShaderState(const std::vector<VkViewport>&					viewports,
																					   const std::vector<VkRect2D>&						scissors,
																					   const VkPipelineLayout							layout,
																					   const VkRenderPass								renderPass,
																					   const deUint32									subpass,
																					   const VkShaderModule								taskShaderModule,
																					   const VkShaderModule								meshShaderModule,
																					   const VkPipelineRasterizationStateCreateInfo*	rasterizationState,
																					   const VkSpecializationInfo						*taskSpecializationInfo,
																					   const VkSpecializationInfo						*meshSpecializationInfo,
																					   VkPipelineFragmentShadingRateStateCreateInfoKHR*	fragmentShadingRateState,
																					   PipelineRenderingCreateInfoWrapper				rendering,
																					   const VkPipelineCache							partPipelineCache,
																					   VkPipelineCreationFeedbackCreateInfoEXT			*partCreationFeedback)
{
	// Make sure pipeline was not already built.
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// Make sure states are set in order - this state needs to be set first or second.
	DE_ASSERT(m_internalData && (m_internalData->setupState < PSS_PRE_RASTERIZATION_SHADERS));

	// The vertex input interface is not needed for mesh shading pipelines, so we're going to mark it as ready here.
	m_internalData->setupState					|= (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS);
	m_internalData->pFragmentShadingRateState	= fragmentShadingRateState;
	m_internalData->pRenderingState				= rendering;

	const bool hasTask				= (taskShaderModule != DE_NULL);
	const auto taskShaderCount		= static_cast<uint32_t>(hasTask);
	const auto pRasterizationState	= rasterizationState
									? rasterizationState
									: (m_internalData->useDefaultRasterizationState
										? &m_internalData->defaultRasterizationState
										: nullptr);
	const auto pTessellationState	= nullptr;
	const auto pViewportState		= m_internalData->useViewportState ? &m_internalData->viewportState : DE_NULL;

	// Reserve space for all stages including fragment. This is needed when we create monolithic pipeline.
	m_internalData->pipelineShaderStages = std::vector<VkPipelineShaderStageCreateInfo>(2u + taskShaderCount,
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
		nullptr,												// const void*							pNext
		0u,														// VkPipelineShaderStageCreateFlags		flags
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage
		DE_NULL,												// VkShaderModule						module
		"main",													// const char*							pName
		nullptr,												// const VkSpecializationInfo*			pSpecializationInfo
	});

	// Mesh shader.
	auto currStage = m_internalData->pipelineShaderStages.begin();
	{
		auto& stageInfo = *currStage;

		stageInfo.stage					= VK_SHADER_STAGE_MESH_BIT_EXT;
		stageInfo.module				= meshShaderModule;
		stageInfo.pSpecializationInfo	= meshSpecializationInfo;

		++currStage;
	}

	if (hasTask)
	{
		auto& stageInfo = *currStage;

		stageInfo.stage					= VK_SHADER_STAGE_TASK_BIT_EXT;
		stageInfo.module				= taskShaderModule;
		stageInfo.pSpecializationInfo	= taskSpecializationInfo;

		++currStage;
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
		m_internalData->monolithicPipelineCreateInfo.stageCount				= 1u + taskShaderCount;
		m_internalData->monolithicPipelineCreateInfo.pStages				= m_internalData->pipelineShaderStages.data();
		m_internalData->monolithicPipelineCreateInfo.pTessellationState		= pTessellationState;
	}
	else
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
		addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
		addToChain(&firstStructInChain, partCreationFeedback);

		VkPipelineDynamicStateCreateInfo pickedDynamicStateInfo = initVulkanStructure();
		std::vector<VkDynamicState> states;

		if(m_internalData->pDynamicState)
		{
			states = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

			pickedDynamicStateInfo.pDynamicStates = states.data();
			pickedDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
		}


		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext			= firstStructInChain;
		pipelinePartCreateInfo.flags			= m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelinePartCreateInfo.layout				= layout;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pRasterizationState	= pRasterizationState;
		pipelinePartCreateInfo.pViewportState		= pViewportState;
		pipelinePartCreateInfo.stageCount			= 1u + taskShaderCount;
		pipelinePartCreateInfo.pStages				= m_internalData->pipelineShaderStages.data();
		pipelinePartCreateInfo.pTessellationState	= pTessellationState;
		pipelinePartCreateInfo.pDynamicState		= &pickedDynamicStateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[1] = createGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}

	return *this;
}
#endif // CTS_USES_VULKANSC

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupFragmentShaderState(const VkPipelineLayout							layout,
																		   const VkRenderPass								renderPass,
																		   const deUint32									subpass,
																		   const VkShaderModule								fragmentShaderModule,
																		   const VkPipelineDepthStencilStateCreateInfo*		depthStencilState,
																		   const VkPipelineMultisampleStateCreateInfo*		multisampleState,
																		   const VkSpecializationInfo*						specializationInfo,
																		   const VkPipelineCache							partPipelineCache,
																		   PipelineCreationFeedbackCreateInfoWrapper		partCreationFeedback)
{
	return setupFragmentShaderState2(layout,
									 renderPass,
									 subpass,
									 fragmentShaderModule,
									 PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
									 depthStencilState,
									 multisampleState,
									 specializationInfo,
									 partPipelineCache,
									 partCreationFeedback);
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupFragmentShaderState2(const VkPipelineLayout										layout,
																			const VkRenderPass											renderPass,
																			const deUint32												subpass,
																			const VkShaderModule										fragmentShaderModule,
																			PipelineShaderStageModuleIdentifierCreateInfoWrapper		fragmentShaderModuleId,
																			const VkPipelineDepthStencilStateCreateInfo*				depthStencilState,
																			const VkPipelineMultisampleStateCreateInfo*					multisampleState,
																			const VkSpecializationInfo*									specializationInfo,
																			const VkPipelineCache										partPipelineCache,
																			PipelineCreationFeedbackCreateInfoWrapper					partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set third
	DE_ASSERT(m_internalData && (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS)));

	// Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
	DE_UNREF(layout);
	DE_UNREF(renderPass);
	DE_UNREF(subpass);
	DE_UNREF(partPipelineCache);
	DE_UNREF(partCreationFeedback);
	DE_UNREF(fragmentShaderModuleId);

	m_internalData->setupState |= PSS_FRAGMENT_SHADER;

	const auto pDepthStencilState	= depthStencilState ? depthStencilState
														: (m_internalData->useDefaultDepthStencilState ? &defaultDepthStencilState : DE_NULL);
	const auto pMultisampleState	= multisampleState ? multisampleState
														: (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);
	const bool hasFrag				= (fragmentShaderModule != DE_NULL || fragmentShaderModuleId.ptr);

	VkPipelineCreateFlags shaderModuleIdFlags = 0u;

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
#ifndef CTS_USES_VULKANSC
				if (fragmentShaderModuleId.ptr)
				{
					m_internalData->pipelineShaderIdentifiers.emplace_back(new PipelineShaderStageModuleIdentifierCreateInfoWrapper(fragmentShaderModuleId.ptr));
					m_internalData->pipelineShaderStages[stageIndex].pNext = m_internalData->pipelineShaderIdentifiers.back().get()->ptr;

					if (fragmentShaderModule == DE_NULL)
						shaderModuleIdFlags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
				}
#endif // CTS_USES_VULKANSC
				break;
			}
		}
	}

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pDepthStencilState	= pDepthStencilState;
		m_internalData->monolithicPipelineCreateInfo.pMultisampleState	= pMultisampleState;
		m_internalData->monolithicPipelineCreateInfo.stageCount			+= (hasFrag ? 1u : 0u);
		m_internalData->monolithicPipelineCreateInfo.flags				|= shaderModuleIdFlags;
	}

#ifndef CTS_USES_VULKANSC
	// note we could just use else to if statement above but sinc
	// this section is cut out for Vulkan SC its cleaner with separate if
	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		auto	libraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
		void*	firstStructInChain	= reinterpret_cast<void*>(&libraryCreateInfo);
		addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);
		addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
		addToChain(&firstStructInChain, partCreationFeedback.ptr);
		addToChain(&firstStructInChain, m_internalData->pRepresentativeFragmentTestState.ptr);

		VkPipelineDynamicStateCreateInfo pickedDynamicStateInfo = initVulkanStructure();
		std::vector<VkDynamicState> states;

		if(m_internalData->pDynamicState)
		{
			states = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

			pickedDynamicStateInfo.pDynamicStates = states.data();
			pickedDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
		}


		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | shaderModuleIdFlags) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelinePartCreateInfo.layout				= layout;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pDepthStencilState	= pDepthStencilState;
		pipelinePartCreateInfo.pMultisampleState	= pMultisampleState;
		pipelinePartCreateInfo.stageCount			= hasFrag;
		pipelinePartCreateInfo.pStages				= hasFrag ? &m_internalData->pipelineShaderStages[stageIndex] : DE_NULL;
		pipelinePartCreateInfo.pDynamicState		= &pickedDynamicStateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		if ((shaderModuleIdFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) != 0)
			m_internalData->failOnCompileWhenLinking = true;

		m_pipelineParts[2] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}
#endif // CTS_USES_VULKANSC

	return *this;
}

GraphicsPipelineWrapper& GraphicsPipelineWrapper::setupFragmentOutputState(const VkRenderPass								renderPass,
																		   const deUint32									subpass,
																		   const VkPipelineColorBlendStateCreateInfo*		colorBlendState,
																		   const VkPipelineMultisampleStateCreateInfo*		multisampleState,
																		   const VkPipelineCache							partPipelineCache,
																		   PipelineCreationFeedbackCreateInfoWrapper		partCreationFeedback)
{
	// make sure pipeline was not already build
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure states are set in order - no need to complicate logic to support out of order specification - this state needs to be set last
	DE_ASSERT(m_internalData && (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS | PSS_FRAGMENT_SHADER)));
	m_internalData->setupState |= PSS_FRAGMENT_OUTPUT_INTERFACE;

	// Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
	DE_UNREF(renderPass);
	DE_UNREF(subpass);
	DE_UNREF(partPipelineCache);
	DE_UNREF(partCreationFeedback);

	void* firstStructInChain = DE_NULL;
	addToChain(&firstStructInChain, m_internalData->pFragmentShadingRateState);

#ifndef CTS_USES_VULKANSC
	addToChain(&firstStructInChain, m_internalData->pRenderingState.ptr);
#endif // CTS_USES_VULKANSC

	const auto pColorBlendState		= colorBlendState ? colorBlendState
														: (m_internalData->useDefaultColorBlendState ? &defaultColorBlendState : DE_NULL);
	const auto pMultisampleState	= multisampleState ? multisampleState
														: (m_internalData->useDefaultMultisampleState ? &defaultMultisampleState : DE_NULL);

	if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		m_internalData->monolithicPipelineCreateInfo.pNext				= firstStructInChain;
		m_internalData->monolithicPipelineCreateInfo.flags				|= m_internalData->pipelineFlags;
		m_internalData->monolithicPipelineCreateInfo.pColorBlendState	= pColorBlendState;
		m_internalData->monolithicPipelineCreateInfo.pMultisampleState	= pMultisampleState;
	}

#ifndef CTS_USES_VULKANSC
	// note we could just use else to if statement above but sinc
	// this section is cut out for Vulkan SC its cleaner with separate if
	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		auto libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
		addToChain(&firstStructInChain, &libraryCreateInfo);
		addToChain(&firstStructInChain, partCreationFeedback.ptr);


		VkPipelineDynamicStateCreateInfo pickedDynamicStateInfo = initVulkanStructure();
		std::vector<VkDynamicState> states;

		if(m_internalData->pDynamicState)
		{
			states = getDynamicStates(m_internalData->pDynamicState, m_internalData->setupState);

			pickedDynamicStateInfo.pDynamicStates = states.data();
			pickedDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
		}


		VkGraphicsPipelineCreateInfo pipelinePartCreateInfo = initVulkanStructure();
		pipelinePartCreateInfo.pNext				= firstStructInChain;
		pipelinePartCreateInfo.flags				= (m_internalData->pipelineFlags | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) & ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelinePartCreateInfo.renderPass			= renderPass;
		pipelinePartCreateInfo.subpass				= subpass;
		pipelinePartCreateInfo.pColorBlendState		= pColorBlendState;
		pipelinePartCreateInfo.pMultisampleState	= pMultisampleState;
		pipelinePartCreateInfo.pDynamicState		= &pickedDynamicStateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			pipelinePartCreateInfo.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

		m_pipelineParts[3] = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, partPipelineCache, &pipelinePartCreateInfo);
	}
#endif // CTS_USES_VULKANSC

	return *this;
}

void GraphicsPipelineWrapper::buildPipeline(const VkPipelineCache						pipelineCache,
											const VkPipeline							basePipelineHandle,
											const deInt32								basePipelineIndex,
											PipelineCreationFeedbackCreateInfoWrapper	creationFeedback,
											void*										pNext)
{
	// make sure we are not trying to build pipeline second time
	DE_ASSERT(m_pipelineFinal.get() == DE_NULL);

	// make sure all states were set
	DE_ASSERT(m_internalData && (m_internalData->setupState == (PSS_VERTEX_INPUT_INTERFACE | PSS_PRE_RASTERIZATION_SHADERS |
																PSS_FRAGMENT_SHADER | PSS_FRAGMENT_OUTPUT_INTERFACE)));

	// Unreference variables that are not used in Vulkan SC. No need to put this in ifdef.
	DE_UNREF(creationFeedback);
	DE_UNREF(pNext);

	VkGraphicsPipelineCreateInfo*	pointerToCreateInfo	= &m_internalData->monolithicPipelineCreateInfo;

#ifndef CTS_USES_VULKANSC
	VkGraphicsPipelineCreateInfo	linkedCreateInfo	= initVulkanStructure();
	std::vector<VkPipeline>			rawPipelines;
	VkPipelineLibraryCreateInfoKHR	linkingInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,		// VkStructureType		sType;
		creationFeedback.ptr,									// const void*			pNext;
		0u,														// deUint32				libraryCount;
		nullptr,												// const VkPipeline*	pLibraries;
	};

	if (m_internalData->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		for (const auto& pipelinePtr : m_pipelineParts)
		{
			const auto& pipeline = pipelinePtr.get();
			if (pipeline != DE_NULL)
				rawPipelines.push_back(pipeline);
		}

		linkingInfo.libraryCount	= static_cast<uint32_t>(rawPipelines.size());
		linkingInfo.pLibraries		= de::dataOrNull(rawPipelines);

		linkedCreateInfo.flags		= m_internalData->pipelineFlags;
		linkedCreateInfo.layout		= m_internalData->monolithicPipelineCreateInfo.layout;
		linkedCreateInfo.pNext		= &linkingInfo;

		pointerToCreateInfo			= &linkedCreateInfo;

		if (m_internalData->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
			linkedCreateInfo.flags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;

		if (m_internalData->failOnCompileWhenLinking)
			linkedCreateInfo.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
	}
	else
	{
		// note: there might be other structures in the chain already
		void* firstStructInChain = static_cast<void*>(pointerToCreateInfo);
		addToChain(&firstStructInChain, creationFeedback.ptr);
		addToChain(&firstStructInChain, m_internalData->pRepresentativeFragmentTestState.ptr);
		addToChain(&firstStructInChain, pNext);
	}
#endif // CTS_USES_VULKANSC

	pointerToCreateInfo->basePipelineHandle	= basePipelineHandle;
	pointerToCreateInfo->basePipelineIndex	= basePipelineIndex;

	m_pipelineFinal = makeGraphicsPipeline(m_internalData->vk, m_internalData->device, pipelineCache, pointerToCreateInfo);

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
