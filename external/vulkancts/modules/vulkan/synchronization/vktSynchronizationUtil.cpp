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
 * \brief Synchronization tests utilities
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "deStringUtil.hpp"
#include <set>

namespace vkt
{
namespace synchronization
{
using namespace vk;

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

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule,
									  const VkSpecializationInfo*	specInfo,
									  PipelineCacheData&			pipelineCacheData)
{
	const VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		specInfo,												// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags			flags;
		shaderStageInfo,									// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};

	{
		const vk::Unique<vk::VkPipelineCache>	pipelineCache	(pipelineCacheData.createPipelineCache(vk, device));
		vk::Move<vk::VkPipeline>				pipeline		(createComputePipeline(vk, device, *pipelineCache, &pipelineInfo));

		// Refresh data from cache
		pipelineCacheData.setFromPipelineCache(vk, device, *pipelineCache);

		return pipeline;
	}
}

VkImageCreateInfo makeImageCreateInfo (const VkImageType			imageType,
									   const VkExtent3D&			extent,
									   const VkFormat				format,
									   const VkImageUsageFlags		usage,
									   const VkSampleCountFlagBits	samples)
{
	return
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags       flags;
		imageType,									// VkImageType              imageType;
		format,										// VkFormat                 format;
		extent,										// VkExtent3D               extent;
		1u,											// uint32_t                 mipLevels;
		1u,											// uint32_t                 arrayLayers;
		samples,									// VkSampleCountFlagBits    samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling            tiling;
		usage,										// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
		0u,											// uint32_t                 queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
	};
}

void beginRenderPassWithRasterizationDisabled (const DeviceInterface&	vk,
											   const VkCommandBuffer	commandBuffer,
											   const VkRenderPass		renderPass,
											   const VkFramebuffer		framebuffer)
{
	const VkRect2D renderArea = {{ 0, 0 }, { 0, 0 }};

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea);
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
												 const VkRenderPass		renderPass,
												 PipelineCacheData&		pipelineCacheData)
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

	const VkPipelineTessellationStateCreateInfo pipelineTessellationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags      flags;
		m_patchControlPoints,											// uint32_t                                    patchControlPoints;
	};

	const VkViewport	viewport	= makeViewport(m_renderSize);
	const VkRect2D		scissor		= makeRect2D(m_renderSize);

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
		DE_NULL,																// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,															// VkPipelineLayout									layout;
		renderPass,																// VkRenderPass										renderPass;
		0u,																		// deUint32											subpass;
		DE_NULL,																// VkPipeline										basePipelineHandle;
		0,																		// deInt32											basePipelineIndex;
	};

	{
		const vk::Unique<vk::VkPipelineCache>	pipelineCache	(pipelineCacheData.createPipelineCache(vk, device));
		vk::Move<vk::VkPipeline>				pipeline		(createGraphicsPipeline(vk, device, *pipelineCache, &graphicsPipelineInfo));

		// Refresh data from cache
		pipelineCacheData.setFromPipelineCache(vk, device, *pipelineCache);

		return pipeline;
	}
}

// Uses some structures added by VK_KHR_synchronization2 to fill legacy structures.
// With this approach we dont have to create branch in each test (one for legacy
// second for new synchronization), this helps to reduce code of some tests.
class LegacySynchronizationWrapper : public SynchronizationWrapperBase
{
protected:

	struct SubmitInfoData
	{
		deUint32		waitSemaphoreCount;
		std::size_t		waitSemaphoreIndex;
		std::size_t		waitSemaphoreValueIndexPlusOne;
		deUint32		commandBufferCount;
		deUint32		commandBufferIndex;
		deUint32		signalSemaphoreCount;
		std::size_t		signalSemaphoreIndex;
		std::size_t		signalSemaphoreValueIndexPlusOne;
	};

	bool isStageFlagAllowed(VkPipelineStageFlags2KHR stage) const
	{
		// synchronization2 suports more stages then legacy synchronization
		// and so SynchronizationWrapper can only be used for cases that
		// operate on stages also supported by legacy synchronization
		// NOTE: if some tests hits assertion that uses this method then this
		// test should not use synchronizationWrapper - it should be synchronization2 exclusive

		static const std::set<deUint32> allowedStages
		{
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
			VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
			VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
			VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
			VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
			VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
			VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
			VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
			VK_PIPELINE_STAGE_NONE_KHR,
		};

		if (stage > static_cast<deUint64>(std::numeric_limits<deUint32>::max()))
			return false;

		return (allowedStages.find(static_cast<deUint32>(stage)) != allowedStages.end());
	}

	bool isAccessFlagAllowed(VkAccessFlags2KHR access) const
	{
		// synchronization2 suports more access flags then legacy synchronization
		// and so SynchronizationWrapper can only be used for cases that
		// operate on access flags also supported by legacy synchronization
		// NOTE: if some tests hits assertion that uses this method then this
		// test should not use synchronizationWrapper - it should be synchronization2 exclusive

		static const std::set<deUint32> allowedAccessFlags
		{
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			VK_ACCESS_INDEX_READ_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_ACCESS_UNIFORM_READ_BIT,
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,
			VK_ACCESS_HOST_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_ACCESS_MEMORY_WRITE_BIT,
			VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
			VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
			VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
			VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
			VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV ,
			VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
			VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
			VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
			VK_ACCESS_NONE_KHR,
		};

		if (access > static_cast<deUint64>(std::numeric_limits<deUint32>::max()))
			return false;

		return (allowedAccessFlags.find(static_cast<deUint32>(access)) != allowedAccessFlags.end());
	}

public:
	LegacySynchronizationWrapper(const DeviceInterface& vk, bool usingTimelineSemaphores, deUint32 submitInfoCount = 1u)
		: SynchronizationWrapperBase	(vk)
		, m_submited					(DE_FALSE)
	{
		m_waitSemaphores.reserve(submitInfoCount);
		m_signalSemaphores.reserve(submitInfoCount);
		m_waitDstStageMasks.reserve(submitInfoCount);
		m_commandBuffers.reserve(submitInfoCount);
		m_submitInfoData.reserve(submitInfoCount);

		if (usingTimelineSemaphores)
			m_timelineSemaphoreValues.reserve(2 * submitInfoCount);
	}

	~LegacySynchronizationWrapper() = default;

	void addSubmitInfo(deUint32								waitSemaphoreInfoCount,
					   const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos,
					   deUint32								commandBufferInfoCount,
					   const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos,
					   deUint32								signalSemaphoreInfoCount,
					   const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos,
					   bool									usingWaitTimelineSemaphore,
					   bool									usingSignalTimelineSemaphore) override
	{
		m_submitInfoData.push_back(SubmitInfoData{ waitSemaphoreInfoCount, 0, 0, commandBufferInfoCount, 0u, signalSemaphoreInfoCount, 0, 0 });
		SubmitInfoData& si = m_submitInfoData.back();

		// memorize wait values
		if (usingWaitTimelineSemaphore)
		{
			DE_ASSERT(pWaitSemaphoreInfos);
			si.waitSemaphoreValueIndexPlusOne = m_timelineSemaphoreValues.size() + 1;
			for (deUint32 i = 0; i < waitSemaphoreInfoCount; ++i)
				m_timelineSemaphoreValues.push_back(pWaitSemaphoreInfos[i].value);
		}

		// memorize signal values
		if (usingSignalTimelineSemaphore)
		{
			DE_ASSERT(pSignalSemaphoreInfos);
			si.signalSemaphoreValueIndexPlusOne = m_timelineSemaphoreValues.size() + 1;
			for (deUint32 i = 0; i < signalSemaphoreInfoCount; ++i)
				m_timelineSemaphoreValues.push_back(pSignalSemaphoreInfos[i].value);
		}

		// construct list of semaphores that we need to wait on
		if (waitSemaphoreInfoCount)
		{
			si.waitSemaphoreIndex = m_waitSemaphores.size();
			for (deUint32 i = 0; i < waitSemaphoreInfoCount; ++i)
			{
				DE_ASSERT(isStageFlagAllowed(pWaitSemaphoreInfos[i].stageMask));
				m_waitSemaphores.push_back(pWaitSemaphoreInfos[i].semaphore);
				m_waitDstStageMasks.push_back(static_cast<VkPipelineStageFlags>(pWaitSemaphoreInfos[i].stageMask));
			}
		}

		// construct list of command buffers
		if (commandBufferInfoCount)
		{
			si.commandBufferIndex = static_cast<deUint32>(m_commandBuffers.size());
			for (deUint32 i = 0; i < commandBufferInfoCount; ++i)
				m_commandBuffers.push_back(pCommandBufferInfos[i].commandBuffer);
		}

		// construct list of semaphores that will be signaled
		if (signalSemaphoreInfoCount)
		{
			si.signalSemaphoreIndex = m_signalSemaphores.size();
			for (deUint32 i = 0; i < signalSemaphoreInfoCount; ++i)
				m_signalSemaphores.push_back(pSignalSemaphoreInfos[i].semaphore);
		}
	}

	void cmdPipelineBarrier(VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		DE_ASSERT(pDependencyInfo);

		VkPipelineStageFlags	srcStageMask				= VK_PIPELINE_STAGE_NONE_KHR;
		VkPipelineStageFlags	dstStageMask				= VK_PIPELINE_STAGE_NONE_KHR;
		deUint32				memoryBarrierCount			= pDependencyInfo->memoryBarrierCount;
		VkMemoryBarrier*		pMemoryBarriers				= DE_NULL;
		deUint32				bufferMemoryBarrierCount	= pDependencyInfo->bufferMemoryBarrierCount;
		VkBufferMemoryBarrier*	pBufferMemoryBarriers		= DE_NULL;
		deUint32				imageMemoryBarrierCount		= pDependencyInfo->imageMemoryBarrierCount;
		VkImageMemoryBarrier*	pImageMemoryBarriers		= DE_NULL;

		// translate VkMemoryBarrier2KHR to VkMemoryBarrier
		std::vector<VkMemoryBarrier> memoryBarriers;
		if (memoryBarrierCount)
		{
			memoryBarriers.reserve(memoryBarrierCount);
			for (deUint32 i = 0; i < memoryBarrierCount; ++i)
			{
				const VkMemoryBarrier2KHR& pMemoryBarrier = pDependencyInfo->pMemoryBarriers[i];

				DE_ASSERT(isStageFlagAllowed(pMemoryBarrier.srcStageMask));
				DE_ASSERT(isStageFlagAllowed(pMemoryBarrier.dstStageMask));
				DE_ASSERT(isAccessFlagAllowed(pMemoryBarrier.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(pMemoryBarrier.dstAccessMask));

				srcStageMask |= static_cast<VkPipelineStageFlags>(pMemoryBarrier.srcStageMask);
				dstStageMask |= static_cast<VkPipelineStageFlags>(pMemoryBarrier.dstStageMask);
				memoryBarriers.push_back(makeMemoryBarrier(
					static_cast<VkAccessFlags>(pMemoryBarrier.srcAccessMask),
					static_cast<VkAccessFlags>(pMemoryBarrier.dstAccessMask)
				));
			}
			pMemoryBarriers = &memoryBarriers[0];
		}

		// translate VkBufferMemoryBarrier2KHR to VkBufferMemoryBarrier
		std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
		if (bufferMemoryBarrierCount)
		{
			bufferMemoryBarriers.reserve(bufferMemoryBarrierCount);
			for (deUint32 i = 0; i < bufferMemoryBarrierCount; ++i)
			{
				const VkBufferMemoryBarrier2KHR& pBufferMemoryBarrier = pDependencyInfo->pBufferMemoryBarriers[i];

				DE_ASSERT(isStageFlagAllowed(pBufferMemoryBarrier.srcStageMask));
				DE_ASSERT(isStageFlagAllowed(pBufferMemoryBarrier.dstStageMask));
				DE_ASSERT(isAccessFlagAllowed(pBufferMemoryBarrier.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(pBufferMemoryBarrier.dstAccessMask));

				srcStageMask |= static_cast<VkPipelineStageFlags>(pBufferMemoryBarrier.srcStageMask);
				dstStageMask |= static_cast<VkPipelineStageFlags>(pBufferMemoryBarrier.dstStageMask);
				bufferMemoryBarriers.push_back(makeBufferMemoryBarrier(
					static_cast<VkAccessFlags>(pBufferMemoryBarrier.srcAccessMask),
					static_cast<VkAccessFlags>(pBufferMemoryBarrier.dstAccessMask),
					pBufferMemoryBarrier.buffer,
					pBufferMemoryBarrier.offset,
					pBufferMemoryBarrier.size,
					pBufferMemoryBarrier.srcQueueFamilyIndex,
					pBufferMemoryBarrier.dstQueueFamilyIndex
				));
			}
			pBufferMemoryBarriers = &bufferMemoryBarriers[0];
		}

		// translate VkImageMemoryBarrier2KHR to VkImageMemoryBarrier
		std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
		if (imageMemoryBarrierCount)
		{
			imageMemoryBarriers.reserve(imageMemoryBarrierCount);
			for (deUint32 i = 0; i < imageMemoryBarrierCount; ++i)
			{
				const VkImageMemoryBarrier2KHR& pImageMemoryBarrier = pDependencyInfo->pImageMemoryBarriers[i];

				DE_ASSERT(isStageFlagAllowed(pImageMemoryBarrier.srcStageMask));
				DE_ASSERT(isStageFlagAllowed(pImageMemoryBarrier.dstStageMask));
				DE_ASSERT(isAccessFlagAllowed(pImageMemoryBarrier.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(pImageMemoryBarrier.dstAccessMask));

				srcStageMask |= static_cast<VkPipelineStageFlags>(pImageMemoryBarrier.srcStageMask);
				dstStageMask |= static_cast<VkPipelineStageFlags>(pImageMemoryBarrier.dstStageMask);
				imageMemoryBarriers.push_back(makeImageMemoryBarrier(
					static_cast<VkAccessFlags>(pImageMemoryBarrier.srcAccessMask),
					static_cast<VkAccessFlags>(pImageMemoryBarrier.dstAccessMask),
					pImageMemoryBarrier.oldLayout,
					pImageMemoryBarrier.newLayout,
					pImageMemoryBarrier.image,
					pImageMemoryBarrier.subresourceRange,
					pImageMemoryBarrier.srcQueueFamilyIndex,
					pImageMemoryBarrier.dstQueueFamilyIndex
				));
			}
			pImageMemoryBarriers = &imageMemoryBarriers[0];
		}

		m_vk.cmdPipelineBarrier(
			commandBuffer,
			srcStageMask,
			dstStageMask,
			(VkDependencyFlags)0,
			memoryBarrierCount,
			pMemoryBarriers,
			bufferMemoryBarrierCount,
			pBufferMemoryBarriers,
			imageMemoryBarrierCount,
			pImageMemoryBarriers
		);
	}

	void cmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		DE_ASSERT(pDependencyInfo);

		VkPipelineStageFlags2KHR srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
		if (pDependencyInfo->pMemoryBarriers)
			srcStageMask = pDependencyInfo->pMemoryBarriers[0].srcStageMask;
		if (pDependencyInfo->pBufferMemoryBarriers)
			srcStageMask = pDependencyInfo->pBufferMemoryBarriers[0].srcStageMask;
		if (pDependencyInfo->pImageMemoryBarriers)
			srcStageMask = pDependencyInfo->pImageMemoryBarriers[0].srcStageMask;

		DE_ASSERT(isStageFlagAllowed(srcStageMask));
		m_vk.cmdSetEvent(commandBuffer, event, static_cast<VkPipelineStageFlags>(srcStageMask));
	}

	void cmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2KHR flag) const override
	{
		DE_ASSERT(isStageFlagAllowed(flag));
		VkPipelineStageFlags legacyStageMask = static_cast<VkPipelineStageFlags>(flag);
		m_vk.cmdResetEvent(commandBuffer, event, legacyStageMask);
	}

	void cmdWaitEvents(VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		DE_ASSERT(pDependencyInfo);

		VkPipelineStageFlags2KHR			srcStageMask				= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
		VkPipelineStageFlags2KHR			dstStageMask				= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
		deUint32							memoryBarrierCount			= pDependencyInfo->memoryBarrierCount;
		deUint32							bufferMemoryBarrierCount	= pDependencyInfo->bufferMemoryBarrierCount;
		deUint32							imageMemoryBarrierCount		= pDependencyInfo->imageMemoryBarrierCount;
		VkMemoryBarrier*					pMemoryBarriers				= DE_NULL;
		VkBufferMemoryBarrier*				pBufferMemoryBarriers		= DE_NULL;
		VkImageMemoryBarrier*				pImageMemoryBarriers		= DE_NULL;
		std::vector<VkMemoryBarrier>		memoryBarriers;
		std::vector<VkBufferMemoryBarrier>	bufferMemoryBarriers;
		std::vector<VkImageMemoryBarrier>	imageMemoryBarriers;

		if (pDependencyInfo->pMemoryBarriers)
		{
			srcStageMask = pDependencyInfo->pMemoryBarriers[0].srcStageMask;
			dstStageMask = pDependencyInfo->pMemoryBarriers[0].dstStageMask;

			memoryBarriers.reserve(memoryBarrierCount);
			for (deUint32 i = 0; i < memoryBarrierCount; ++i)
			{
				const VkMemoryBarrier2KHR& mb = pDependencyInfo->pMemoryBarriers[i];
				DE_ASSERT(isAccessFlagAllowed(mb.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(mb.dstAccessMask));
				memoryBarriers.push_back(
					makeMemoryBarrier(
						static_cast<VkAccessFlags>(mb.srcAccessMask),
						static_cast<VkAccessFlags>(mb.dstAccessMask)
					)
				);
			}
			pMemoryBarriers = &memoryBarriers[0];
		}
		if (pDependencyInfo->pBufferMemoryBarriers)
		{
			srcStageMask = pDependencyInfo->pBufferMemoryBarriers[0].srcStageMask;
			dstStageMask = pDependencyInfo->pBufferMemoryBarriers[0].dstStageMask;

			bufferMemoryBarriers.reserve(bufferMemoryBarrierCount);
			for (deUint32 i = 0; i < bufferMemoryBarrierCount; ++i)
			{
				const VkBufferMemoryBarrier2KHR& bmb = pDependencyInfo->pBufferMemoryBarriers[i];
				DE_ASSERT(isAccessFlagAllowed(bmb.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(bmb.dstAccessMask));
				bufferMemoryBarriers.push_back(
					makeBufferMemoryBarrier(
						static_cast<VkAccessFlags>(bmb.srcAccessMask),
						static_cast<VkAccessFlags>(bmb.dstAccessMask),
						bmb.buffer,
						bmb.offset,
						bmb.size,
						bmb.srcQueueFamilyIndex,
						bmb.dstQueueFamilyIndex
					)
				);
			}
			pBufferMemoryBarriers = &bufferMemoryBarriers[0];
		}
		if (pDependencyInfo->pImageMemoryBarriers)
		{
			srcStageMask = pDependencyInfo->pImageMemoryBarriers[0].srcStageMask;
			dstStageMask = pDependencyInfo->pImageMemoryBarriers[0].dstStageMask;

			imageMemoryBarriers.reserve(imageMemoryBarrierCount);
			for (deUint32 i = 0; i < imageMemoryBarrierCount; ++i)
			{
				const VkImageMemoryBarrier2KHR& imb = pDependencyInfo->pImageMemoryBarriers[i];
				DE_ASSERT(isAccessFlagAllowed(imb.srcAccessMask));
				DE_ASSERT(isAccessFlagAllowed(imb.dstAccessMask));
				imageMemoryBarriers.push_back(
					makeImageMemoryBarrier(
						static_cast<VkAccessFlags>(imb.srcAccessMask),
						static_cast<VkAccessFlags>(imb.dstAccessMask),
						imb.oldLayout,
						imb.newLayout,
						imb.image,
						imb.subresourceRange,
						imb.srcQueueFamilyIndex,
						imb.dstQueueFamilyIndex
					)
				);
			}
			pImageMemoryBarriers = &imageMemoryBarriers[0];
		}

		DE_ASSERT(isStageFlagAllowed(srcStageMask));
		DE_ASSERT(isStageFlagAllowed(dstStageMask));
		m_vk.cmdWaitEvents(commandBuffer, eventCount, pEvents,
			static_cast<VkPipelineStageFlags>(srcStageMask), static_cast<VkPipelineStageFlags>(dstStageMask),
			memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
	}

	VkResult queueSubmit(VkQueue queue, VkFence fence) override
	{
		// make sure submit info was added
		DE_ASSERT(!m_submitInfoData.empty());

		// make sure separate LegacySynchronizationWrapper is created per single submit
		DE_ASSERT(!m_submited);

		std::vector<VkSubmitInfo> submitInfo(m_submitInfoData.size(), { VK_STRUCTURE_TYPE_SUBMIT_INFO, DE_NULL, 0u, DE_NULL, DE_NULL, 0u, DE_NULL, 0u, DE_NULL });

		std::vector<VkTimelineSemaphoreSubmitInfo> timelineSemaphoreSubmitInfo;
		timelineSemaphoreSubmitInfo.reserve(m_submitInfoData.size());

		// translate indexes from m_submitInfoData to pointers and construct VkSubmitInfo
		for (deUint32 i = 0; i < m_submitInfoData.size(); ++i)
		{
			auto&			data	= m_submitInfoData[i];
			VkSubmitInfo&	si		= submitInfo[i];

			si.waitSemaphoreCount	= data.waitSemaphoreCount;
			si.commandBufferCount	= data.commandBufferCount;
			si.signalSemaphoreCount	= data.signalSemaphoreCount;

			if (data.waitSemaphoreValueIndexPlusOne || data.signalSemaphoreValueIndexPlusOne)
			{
				deUint64* pWaitSemaphoreValues = DE_NULL;
				if (data.waitSemaphoreValueIndexPlusOne)
					pWaitSemaphoreValues = &m_timelineSemaphoreValues[data.waitSemaphoreValueIndexPlusOne - 1];

				deUint64* pSignalSemaphoreValues = DE_NULL;
				if (data.signalSemaphoreValueIndexPlusOne)
					pSignalSemaphoreValues = &m_timelineSemaphoreValues[data.signalSemaphoreValueIndexPlusOne - 1];

				timelineSemaphoreSubmitInfo.push_back({
					VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
					DE_NULL,												// const void*		pNext;
					data.waitSemaphoreCount,								// deUint32			waitSemaphoreValueCount
					pWaitSemaphoreValues,									// const deUint64*	pWaitSemaphoreValues
					data.signalSemaphoreCount,								// deUint32			signalSemaphoreValueCount
					pSignalSemaphoreValues									// const deUint64*	pSignalSemaphoreValues
				});
				si.pNext = &timelineSemaphoreSubmitInfo.back();
			}

			if (data.waitSemaphoreCount)
			{
				si.pWaitSemaphores		= &m_waitSemaphores[data.waitSemaphoreIndex];
				si.pWaitDstStageMask	= &m_waitDstStageMasks[data.waitSemaphoreIndex];
			}

			if (data.commandBufferCount)
				si.pCommandBuffers = &m_commandBuffers[data.commandBufferIndex];

			if (data.signalSemaphoreCount)
				si.pSignalSemaphores = &m_signalSemaphores[data.signalSemaphoreIndex];
		}

		m_submited = DE_TRUE;
		return m_vk.queueSubmit(queue, static_cast<deUint32>(submitInfo.size()), &submitInfo[0], fence);
	}

protected:

	std::vector<VkSemaphore>			m_waitSemaphores;
	std::vector<VkSemaphore>			m_signalSemaphores;
	std::vector<VkPipelineStageFlags>	m_waitDstStageMasks;
	std::vector<VkCommandBuffer>		m_commandBuffers;
	std::vector<SubmitInfoData>			m_submitInfoData;
	std::vector<deUint64>				m_timelineSemaphoreValues;
	bool								m_submited;
};

class Synchronization2Wrapper : public SynchronizationWrapperBase
{
public:
	Synchronization2Wrapper(const DeviceInterface& vk, deUint32 submitInfoCount)
		: SynchronizationWrapperBase(vk)
	{
		m_submitInfo.reserve(submitInfoCount);
	}

	~Synchronization2Wrapper() = default;

	void addSubmitInfo(deUint32								waitSemaphoreInfoCount,
					   const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos,
					   deUint32								commandBufferInfoCount,
					   const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos,
					   deUint32								signalSemaphoreInfoCount,
					   const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos,
					   bool									usingWaitTimelineSemaphore,
					   bool									usingSignalTimelineSemaphore) override
	{
		DE_UNREF(usingWaitTimelineSemaphore);
		DE_UNREF(usingSignalTimelineSemaphore);

		m_submitInfo.push_back(VkSubmitInfo2KHR{
			VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,		// VkStructureType						sType
			DE_NULL,									// const void*							pNext
			0u,											// VkSubmitFlagsKHR						flags
			waitSemaphoreInfoCount,						// deUint32								waitSemaphoreInfoCount
			pWaitSemaphoreInfos,						// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			commandBufferInfoCount,						// deUint32								commandBufferInfoCount
			pCommandBufferInfos,						// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			signalSemaphoreInfoCount,					// deUint32								signalSemaphoreInfoCount
			pSignalSemaphoreInfos						// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
		});
	}

	void cmdPipelineBarrier(VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		m_vk.cmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo);
	}

	void cmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		m_vk.cmdSetEvent2KHR(commandBuffer, event, pDependencyInfo);
	}

	void cmdWaitEvents(VkCommandBuffer commandBuffer, deUint32 eventCount, const VkEvent* pEvents, const VkDependencyInfoKHR* pDependencyInfo) const override
	{
		m_vk.cmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfo);
	}

	void cmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2KHR flag) const override
	{
		m_vk.cmdResetEvent2KHR(commandBuffer, event, flag);
	}

	VkResult queueSubmit(VkQueue queue, VkFence fence) override
	{
		return m_vk.queueSubmit2KHR(queue, static_cast<deUint32>(m_submitInfo.size()), &m_submitInfo[0], fence);
	}

protected:

	std::vector<VkSubmitInfo2KHR> m_submitInfo;
};

SynchronizationWrapperPtr getSynchronizationWrapper(SynchronizationType		type,
													const DeviceInterface&	vk,
													bool					usingTimelineSemaphores,
													deUint32				submitInfoCount)
{
	return (type == SynchronizationType::LEGACY)
		? SynchronizationWrapperPtr(new LegacySynchronizationWrapper(vk, usingTimelineSemaphores, submitInfoCount))
		: SynchronizationWrapperPtr(new Synchronization2Wrapper(vk, submitInfoCount));
}

void submitCommandsAndWait(SynchronizationWrapperPtr	synchronizationWrapper,
						   const DeviceInterface&		vk,
						   const VkDevice				device,
						   const VkQueue				queue,
						   const VkCommandBuffer		cmdBuffer)
{
	VkCommandBufferSubmitInfoKHR commandBufferInfoCount = makeCommonCommandBufferSubmitInfo(cmdBuffer);

	synchronizationWrapper->addSubmitInfo(
		0u,										// deUint32								waitSemaphoreInfoCount
		DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
		1u,										// deUint32								commandBufferInfoCount
		&commandBufferInfoCount,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
		0u,										// deUint32								signalSemaphoreInfoCount
		DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
	);

	const Unique<VkFence> fence(createFence(vk, device));
	VK_CHECK(synchronizationWrapper->queueSubmit(queue, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
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

void requireStorageImageSupport(const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkFormat fmt)
{
	const VkFormatProperties p = getPhysicalDeviceFormatProperties(vki, physDevice, fmt);
	if ((p.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		throw tcu::NotSupportedError("Storage image format not supported");
}

std::string getResourceName (const ResourceDescription& resource)
{
	std::ostringstream str;

	if ((resource.type == RESOURCE_TYPE_BUFFER) ||
		(resource.type == RESOURCE_TYPE_INDEX_BUFFER))
	{
		str << "buffer_" << resource.size.x();
	}
	else if (resource.type == RESOURCE_TYPE_IMAGE)
	{
		str << "image_" << resource.size.x()
						<< (resource.size.y() > 0 ? "x" + de::toString(resource.size.y()) : "")
						<< (resource.size.z() > 0 ? "x" + de::toString(resource.size.z()) : "")
			<< "_" << de::toLower(getFormatName(resource.imageFormat)).substr(10);
	}
	else if (isIndirectBuffer(resource.type))
		str << "indirect_buffer";
	else
		DE_ASSERT(0);

	return str.str();
}

bool isIndirectBuffer (const ResourceType type)
{
	switch (type)
	{
		case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW:
		case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED:
		case RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH:
			return true;

		default:
			return false;
	}
}

VkCommandBufferSubmitInfoKHR makeCommonCommandBufferSubmitInfo (const VkCommandBuffer cmdBuf)
{
	return
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,	// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		cmdBuf,												// VkCommandBuffer		commandBuffer
		0u													// uint32_t				deviceMask
	};
}

VkSemaphoreSubmitInfoKHR makeCommonSemaphoreSubmitInfo(VkSemaphore semaphore, deUint64 value, VkPipelineStageFlags2KHR stageMask)
{
	return
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,	// VkStructureType				sType
		DE_NULL,										// const void*					pNext
		semaphore,										// VkSemaphore					semaphore
		value,											// deUint64						value
		stageMask,										// VkPipelineStageFlags2KHR		stageMask
		0u												// deUint32						deviceIndex
	};
}

VkDependencyInfoKHR makeCommonDependencyInfo(const VkMemoryBarrier2KHR* pMemoryBarrier, const VkBufferMemoryBarrier2KHR* pBufferMemoryBarrier, const VkImageMemoryBarrier2KHR* pImageMemoryBarrier)
{
	return
	{
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,		// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		VK_DEPENDENCY_BY_REGION_BIT,				// VkDependencyFlags				dependencyFlags
		!!pMemoryBarrier,							// deUint32							memoryBarrierCount
		pMemoryBarrier,								// const VkMemoryBarrier2KHR*		pMemoryBarriers
		!!pBufferMemoryBarrier,						// deUint32							bufferMemoryBarrierCount
		pBufferMemoryBarrier,						// const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers
		!!pImageMemoryBarrier,						// deUint32							imageMemoryBarrierCount
		pImageMemoryBarrier							// const VkImageMemoryBarrier2KHR*	pImageMemoryBarriers
	};
};

PipelineCacheData::PipelineCacheData (void)
{
}

PipelineCacheData::~PipelineCacheData (void)
{
}

vk::Move<VkPipelineCache> PipelineCacheData::createPipelineCache (const vk::DeviceInterface& vk, const vk::VkDevice device) const
{
	const de::ScopedLock						dataLock	(m_lock);
	const struct vk::VkPipelineCacheCreateInfo	params	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineCacheCreateFlags)0,
		(deUintptr)m_data.size(),
		(m_data.empty() ? DE_NULL : &m_data[0])
	};

	return vk::createPipelineCache(vk, device, &params);
}

void PipelineCacheData::setFromPipelineCache (const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineCache pipelineCache)
{
	const de::ScopedLock		dataLock		(m_lock);
	deUintptr					dataSize		= 0;

	VK_CHECK(vk.getPipelineCacheData(device, pipelineCache, &dataSize, DE_NULL));

	m_data.resize(dataSize);

	if (dataSize > 0)
		VK_CHECK(vk.getPipelineCacheData(device, pipelineCache, &dataSize, &m_data[0]));
}

} // synchronization
} // vkt
