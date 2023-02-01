#ifndef _VKPIPELINECONSTRUCTIONUTIL_HPP
#define _VKPIPELINECONSTRUCTIONUTIL_HPP
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
          VK_EXT_graphics_pipeline_library for pipeline construction.
 *//*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "vkDefs.hpp"
#include "tcuDefs.hpp"
#include "deSharedPtr.hpp"
#include <vector>

namespace vk
{

enum PipelineConstructionType
{
	PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC			= 0,	// Construct legacy - monolithic pipeline
	PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY,	// Use VK_EXT_graphics_pipeline_library and construc pipeline out of 4 pipeline parts
	PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY			// Same as PIPELINE_CONSTRUCTION_TYPE_OPTIMISED_LIBRARY but with fast linking
};

void checkPipelineLibraryRequirements (const InstanceInterface&		vki,
									   VkPhysicalDevice				physicalDevice,
									   PipelineConstructionType		pipelineConstructionType);

// Class that can build monolithic pipeline or fully separated pipeline libraries
// depending on PipelineType specified in the constructor.
// Rarely needed configuration was extracted to setDefault*/disable* functions while common
// state setup is provided as arguments of four setup* functions - one for each state group.
class GraphicsPipelineWrapper
{
public:
								GraphicsPipelineWrapper				(const DeviceInterface&				vk,
																	 VkDevice							device,
																	 const PipelineConstructionType		pipelineConstructionType,
																	 const VkPipelineCreateFlags		flags = 0u);

								GraphicsPipelineWrapper				(GraphicsPipelineWrapper&&) noexcept;

								~GraphicsPipelineWrapper			(void) = default;


	// By default pipelineLayout used for monotlithic pipeline is taken from layout specified
	// in setupPreRasterizationShaderState but when there are also descriptor sets needed for fragment
	// shader bindings then separate pipeline layout for monolithic pipeline must be provided
	GraphicsPipelineWrapper&	setMonolithicPipelineLayout			(const VkPipelineLayout layout);


	// By default dynamic state has to be specified before specifying other CreateInfo structures
	GraphicsPipelineWrapper&	setDynamicState						(const VkPipelineDynamicStateCreateInfo* dynamicState);

	// Specify topology that is used by default InputAssemblyState in vertex input state. This needs to be
	// specified only when there is no custom InputAssemblyState provided in setupVertexInputStete and when
	// topology is diferent then VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST which is used by default.
	GraphicsPipelineWrapper&	setDefaultTopology					(const VkPrimitiveTopology topology);

	// Specify patch control points that is used by default TessellationState in pre-rasterization shader state.
	// This can to be specified only when there is no custom TessellationState provided in
	// setupPreRasterizationShaderState and when patchControlPoints is diferent then 3 which is used by default.
	GraphicsPipelineWrapper&	setDefaultPatchControlPoints		(const deUint32 patchControlPoints);

	// Enable discarding of primitives that is used by default RasterizationState in pre-rasterization shader state.
	// This can be specified only when there is no custom RasterizationState provided in setupPreRasterizationShaderState.
	GraphicsPipelineWrapper&	setDefaultRasterizerDiscardEnable	(const deBool rasterizerDiscardEnable = DE_TRUE);

	// When some states are not provided then default structures can be used. This behaviour can be turned on by one of below methods.
	// Some tests require those states to be NULL so we can't assume using default versions.
	GraphicsPipelineWrapper&	setDefaultRasterizationState		(void);
	GraphicsPipelineWrapper&	setDefaultDepthStencilState			(void);
	GraphicsPipelineWrapper&	setDefaultColorBlendState			(void);
	GraphicsPipelineWrapper&	setDefaultMultisampleState			(void);

	// Pre-rasterization shader state uses provieded viewports and scissors to create ViewportState. By default
	// number of viewports and scissors is same as number of items in vector but when vectors are empty then by
	// default count of viewports/scissors is set to 1. This can be changed by below functions.
	GraphicsPipelineWrapper&	setDefaultViewportsCount			(deUint32 viewportCount = 0u);
	GraphicsPipelineWrapper&	setDefaultScissorsCount				(deUint32 scissorCount = 0u);

	// Pre-rasterization shader state uses default ViewportState, this method extends it with VkPipelineViewportDepthClipControlCreateInfoEXT.
	GraphicsPipelineWrapper&	setDepthClipControl					(const VkPipelineViewportDepthClipControlCreateInfoEXT* depthClipControlCreateInfo);

	// Pre-rasterization shader state uses provieded viewports and scissors to create ViewportState. When disableViewportState
	// is used then ViewportState won't be constructed and NULL will be used.
	GraphicsPipelineWrapper&	disableViewportState				(void);


	// Setup vertex input state. When VertexInputState or InputAssemblyState are not provided then default structures will be used.
	GraphicsPipelineWrapper&	setupVertexInputStete				(const VkPipelineVertexInputStateCreateInfo*		vertexInputState = DE_NULL,
																	 const VkPipelineInputAssemblyStateCreateInfo*		inputAssemblyState = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback = DE_NULL);

	// Setup pre-rasterization shader state.
	GraphicsPipelineWrapper&	setupPreRasterizationShaderState	(const std::vector<VkViewport>&						viewports,
																	 const std::vector<VkRect2D>&						scissors,
																	 const VkPipelineLayout								layout,
																	 const VkRenderPass									renderPass,
																	 const deUint32										subpass,
																	 const VkShaderModule								vertexShaderModule,
																	 const VkPipelineRasterizationStateCreateInfo*		rasterizationState = DE_NULL,
																	 const VkShaderModule								tessellationControlShaderModule = DE_NULL,
																	 const VkShaderModule								tessellationEvalShaderModule = DE_NULL,
																	 const VkShaderModule								geometryShaderModule = DE_NULL,
																	 const VkSpecializationInfo*						specializationInfo = DE_NULL,
																	 VkPipelineFragmentShadingRateStateCreateInfoKHR*	fragmentShadingRateState = nullptr,
																	 VkPipelineRenderingCreateInfoKHR*					rendering = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback = DE_NULL);

	// Setup fragment shader state.
	GraphicsPipelineWrapper&	setupFragmentShaderState			(const VkPipelineLayout								layout,
																	 const VkRenderPass									renderPass,
																	 const deUint32										subpass,
																	 const VkShaderModule								fragmentShaderModule,
																	 const VkPipelineDepthStencilStateCreateInfo*		depthStencilState = DE_NULL,
																	 const VkPipelineMultisampleStateCreateInfo*		multisampleState = DE_NULL,
																	 const VkSpecializationInfo*						specializationInfo = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback = DE_NULL);

	// Setup fragment output state.
	GraphicsPipelineWrapper&	setupFragmentOutputState			(const VkRenderPass									renderPass,
																	 const deUint32										subpass = 0u,
																	 const VkPipelineColorBlendStateCreateInfo*			colorBlendState = DE_NULL,
																	 const VkPipelineMultisampleStateCreateInfo*		multisampleState = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback = DE_NULL);

	// Build pipeline object out of provided state.
	void						buildPipeline						(const VkPipelineCache								pipelineCache = DE_NULL,
																	 const VkPipeline									basePipelineHandle = DE_NULL,
																	 const deInt32										basePipelineIndex = 0,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			creationFeedback = DE_NULL);

	// Returns true when pipeline was build using buildPipeline method.
	deBool						wasBuild							(void) const;

	// Get compleate pipeline. GraphicsPipelineWrapper preserves ovnership and will desroy pipeline in its destructor.
	vk::VkPipeline				getPipeline							(void) const;

	// Destroy compleate pipeline - pipeline parts are not destroyed.
	void						destroyPipeline						(void);

protected:

	// No default constructor - use parametrized constructor or emplace_back in case of vectors.
	GraphicsPipelineWrapper() = default;

	struct InternalData;

protected:

	// Store partial pipelines when non monolithic construction was used.
	Move<VkPipeline>				m_pipelineParts[4];

	// Store monolithic pipeline or linked pipeline libraries.
	Move<VkPipeline>				m_pipelineFinal;

	// Store internal data that is needed only for pipeline construction.
	de::SharedPtr<InternalData>		m_internalData;
};

} // vk

#endif // _VKPIPELINECONSTRUCTIONUTIL_HPP
