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
#include <stdexcept>

namespace vk
{

enum PipelineConstructionType
{
	PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC			= 0,	// Construct legacy - monolithic pipeline
	PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY,	// Use VK_EXT_graphics_pipeline_library and construct pipeline out of several pipeline parts.
	PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY			// Same as PIPELINE_CONSTRUCTION_TYPE_OPTIMISED_LIBRARY but with fast linking
};

void checkPipelineLibraryRequirements (const InstanceInterface&		vki,
									   VkPhysicalDevice				physicalDevice,
									   PipelineConstructionType		pipelineConstructionType);

// This exception may be raised in one of the intermediate steps when using shader module IDs instead of normal module objects.
class PipelineCompileRequiredError : public std::runtime_error
{
public:
	PipelineCompileRequiredError (const std::string& msg)
		: std::runtime_error(msg)
		{}
};

// PointerWrapper template is used to hide structures that should not be visible for Vulkan SC
template <typename T>
class PointerWrapper
{
public:
	PointerWrapper(): ptr(DE_NULL)	{}
	PointerWrapper(T* p0) : ptr(p0) {}
	T* ptr;
};

template <typename T>
class ConstPointerWrapper
{
public:
	ConstPointerWrapper(): ptr(DE_NULL)  {}
	ConstPointerWrapper(const T* p0) : ptr(p0) {}
	const T* ptr;
};

#ifndef CTS_USES_VULKANSC
typedef PointerWrapper<VkPipelineViewportDepthClipControlCreateInfoEXT> PipelineViewportDepthClipControlCreateInfoWrapper;
typedef PointerWrapper<VkPipelineRenderingCreateInfoKHR> PipelineRenderingCreateInfoWrapper;
typedef PointerWrapper<VkPipelineCreationFeedbackCreateInfoEXT> PipelineCreationFeedbackCreateInfoWrapper;
typedef ConstPointerWrapper<VkPipelineShaderStageModuleIdentifierCreateInfoEXT> PipelineShaderStageModuleIdentifierCreateInfoWrapper;
typedef PointerWrapper<VkPipelineRepresentativeFragmentTestStateCreateInfoNV> PipelineRepresentativeFragmentTestCreateInfoWrapper;
#else
typedef PointerWrapper<void> PipelineViewportDepthClipControlCreateInfoWrapper;
typedef PointerWrapper<void> PipelineRenderingCreateInfoWrapper;
typedef PointerWrapper<void> PipelineCreationFeedbackCreateInfoWrapper;
typedef ConstPointerWrapper<void> PipelineShaderStageModuleIdentifierCreateInfoWrapper;
typedef PointerWrapper<void> PipelineRepresentativeFragmentTestCreateInfoWrapper;
#endif

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

	// Specify the representative fragment test state.
	GraphicsPipelineWrapper&	setRepresentativeFragmentTestState	(PipelineRepresentativeFragmentTestCreateInfoWrapper representativeFragmentTestState);

	// Specify topology that is used by default InputAssemblyState in vertex input state. This needs to be
	// specified only when there is no custom InputAssemblyState provided in setupVertexInputState and when
	// topology is diferent then VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST which is used by default.
	GraphicsPipelineWrapper&	setDefaultTopology					(const VkPrimitiveTopology topology);

	// Specify patch control points that is used by default TessellationState in pre-rasterization shader state.
	// This can to be specified only when there is no custom TessellationState provided in
	// setupPreRasterizationShaderState and when patchControlPoints is diferent then 3 which is used by default.
	GraphicsPipelineWrapper&	setDefaultPatchControlPoints		(const deUint32 patchControlPoints);

	// Specify tesellation domain origin, used by the tessellation state in pre-rasterization shader state.
	GraphicsPipelineWrapper&	setDefaultTessellationDomainOrigin	(const VkTessellationDomainOrigin domainOrigin, bool forceExtStruct = false);

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

	// Pre-rasterization shader state uses default ViewportState, this method extends the internal structure.
	GraphicsPipelineWrapper&	setViewportStatePnext				(const void* pNext);

#ifndef CTS_USES_VULKANSC
	GraphicsPipelineWrapper&	setRenderingColorAttachmentsInfo	(PipelineRenderingCreateInfoWrapper pipelineRenderingCreateInfo);
#endif

	// Pre-rasterization shader state uses provieded viewports and scissors to create ViewportState. When disableViewportState
	// is used then ViewportState won't be constructed and NULL will be used.
	GraphicsPipelineWrapper&	disableViewportState				(void);


	// Setup vertex input state. When VertexInputState or InputAssemblyState are not provided then default structures will be used.
	GraphicsPipelineWrapper&	setupVertexInputState				(const VkPipelineVertexInputStateCreateInfo*		vertexInputState = DE_NULL,
																	 const VkPipelineInputAssemblyStateCreateInfo*		inputAssemblyState = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper			partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

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
																	 PipelineRenderingCreateInfoWrapper					rendering = PipelineRenderingCreateInfoWrapper(),
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper			partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

	GraphicsPipelineWrapper&	setupPreRasterizationShaderState2	(const std::vector<VkViewport>&						viewports,
																	 const std::vector<VkRect2D>&						scissors,
																	 const VkPipelineLayout								layout,
																	 const VkRenderPass									renderPass,
																	 const deUint32										subpass,
																	 const VkShaderModule								vertexShaderModule,
																	 const VkPipelineRasterizationStateCreateInfo*		rasterizationState = nullptr,
																	 const VkShaderModule								tessellationControlShaderModulnullptre = DE_NULL,
																	 const VkShaderModule								tessellationEvalShaderModule = DE_NULL,
																	 const VkShaderModule								geometryShaderModule = DE_NULL,
																	 const VkSpecializationInfo*						vertSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*						tescSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*						teseSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*						geomSpecializationInfo = nullptr,
																	 VkPipelineFragmentShadingRateStateCreateInfoKHR*	fragmentShadingRateState = nullptr,
																	 PipelineRenderingCreateInfoWrapper					rendering = PipelineRenderingCreateInfoWrapper(),
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper			partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

	// Note: VkPipelineShaderStageModuleIdentifierCreateInfoEXT::pIdentifier will not be copied. They need to continue to exist outside this wrapper.
	GraphicsPipelineWrapper&	setupPreRasterizationShaderState3	(const std::vector<VkViewport>&								viewports,
																	 const std::vector<VkRect2D>&								scissors,
																	 const VkPipelineLayout										layout,
																	 const VkRenderPass											renderPass,
																	 const deUint32												subpass,
																	 const VkShaderModule										vertexShaderModule,
																	 PipelineShaderStageModuleIdentifierCreateInfoWrapper		vertShaderModuleId = PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
																	 const VkPipelineRasterizationStateCreateInfo*				rasterizationState = nullptr,
																	 const VkShaderModule										tessellationControlShaderModule = DE_NULL,
																	 PipelineShaderStageModuleIdentifierCreateInfoWrapper		tescShaderModuleId = PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
																	 const VkShaderModule										tessellationEvalShaderModule = DE_NULL,
																	 PipelineShaderStageModuleIdentifierCreateInfoWrapper		teseShaderModuleId = PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
																	 const VkShaderModule										geometryShaderModule = DE_NULL,
																	 PipelineShaderStageModuleIdentifierCreateInfoWrapper		geomShaderModuleId = PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
																	 const VkSpecializationInfo*								vertSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*								tescSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*								teseSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*								geomSpecializationInfo = nullptr,
																	 VkPipelineFragmentShadingRateStateCreateInfoKHR*			fragmentShadingRateState = nullptr,
																	 PipelineRenderingCreateInfoWrapper							rendering = PipelineRenderingCreateInfoWrapper(),
																	 const VkPipelineCache										partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper					partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

#ifndef CTS_USES_VULKANSC
	// Setup pre-rasterization shader state, mesh shading version.
	GraphicsPipelineWrapper&	setupPreRasterizationMeshShaderState(const std::vector<VkViewport>&						viewports,
																	 const std::vector<VkRect2D>&						scissors,
																	 const VkPipelineLayout								layout,
																	 const VkRenderPass									renderPass,
																	 const deUint32										subpass,
																	 const VkShaderModule								taskShaderModule,
																	 const VkShaderModule								meshShaderModule,
																	 const VkPipelineRasterizationStateCreateInfo*		rasterizationState = nullptr,
																	 const VkSpecializationInfo*						taskSpecializationInfo = nullptr,
																	 const VkSpecializationInfo*						meshSpecializationInfo = nullptr,
																	 VkPipelineFragmentShadingRateStateCreateInfoKHR*	fragmentShadingRateState = nullptr,
																	 PipelineRenderingCreateInfoWrapper					rendering = PipelineRenderingCreateInfoWrapper(),
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 VkPipelineCreationFeedbackCreateInfoEXT*			partCreationFeedback = nullptr);
#endif // CTS_USES_VULKANSC

	// Setup fragment shader state.
	GraphicsPipelineWrapper&	setupFragmentShaderState			(const VkPipelineLayout								layout,
																	 const VkRenderPass									renderPass,
																	 const deUint32										subpass,
																	 const VkShaderModule								fragmentShaderModule,
																	 const VkPipelineDepthStencilStateCreateInfo*		depthStencilState = DE_NULL,
																	 const VkPipelineMultisampleStateCreateInfo*		multisampleState = DE_NULL,
																	 const VkSpecializationInfo*						specializationInfo = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper			partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

	// Note: VkPipelineShaderStageModuleIdentifierCreateInfoEXT::pIdentifier will not be copied. They need to continue to exist outside this wrapper.
	GraphicsPipelineWrapper&	setupFragmentShaderState2			(const VkPipelineLayout										layout,
																	 const VkRenderPass											renderPass,
																	 const deUint32												subpass,
																	 const VkShaderModule										fragmentShaderModule,
																	 PipelineShaderStageModuleIdentifierCreateInfoWrapper		fragmentShaderModuleId = PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
																	 const VkPipelineDepthStencilStateCreateInfo*				depthStencilState = nullptr,
																	 const VkPipelineMultisampleStateCreateInfo*				multisampleState = nullptr,
																	 const VkSpecializationInfo*								specializationInfo = nullptr,
																	 const VkPipelineCache										partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper					partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

	// Setup fragment output state.
	GraphicsPipelineWrapper&	setupFragmentOutputState			(const VkRenderPass									renderPass,
																	 const deUint32										subpass = 0u,
																	 const VkPipelineColorBlendStateCreateInfo*			colorBlendState = DE_NULL,
																	 const VkPipelineMultisampleStateCreateInfo*		multisampleState = DE_NULL,
																	 const VkPipelineCache								partPipelineCache = DE_NULL,
																	 PipelineCreationFeedbackCreateInfoWrapper			partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

	// Build pipeline object out of provided state.
	void						buildPipeline						(const VkPipelineCache								pipelineCache = DE_NULL,
																	 const VkPipeline									basePipelineHandle = DE_NULL,
																	 const deInt32										basePipelineIndex = 0,
																	 PipelineCreationFeedbackCreateInfoWrapper			creationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

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

	static constexpr size_t kMaxPipelineParts = 4u;

	// Store partial pipelines when non monolithic construction was used.
	Move<VkPipeline>				m_pipelineParts[kMaxPipelineParts];

	// Store monolithic pipeline or linked pipeline libraries.
	Move<VkPipeline>				m_pipelineFinal;

	// Store internal data that is needed only for pipeline construction.
	de::SharedPtr<InternalData>		m_internalData;
};

} // vk

#endif // _VKPIPELINECONSTRUCTIONUTIL_HPP
