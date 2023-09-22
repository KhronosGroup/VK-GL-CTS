/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Pipeline Interaction Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectCreateTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkObjUtil.hpp"
#include "deRandom.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum TestType {
	SHADER_OBJECT = 0,
	MAX_PIPELINE,
	MAX_PIPELINE_SHADER_OBJECT_MAX_PIPELINE,
	SHADER_OBJECT_MAX_PIPELINE_SHADER_OBJECT,
	MIN_PIPELINE_SHADER_OBJECT,
	RENDER_PASS_PIPELINE_SHADER_OBJECT,
	RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN,
	SHADER_OBJECT_MIN_PIPELINE,
	COMPUTE_SHADER_OBJECT_MIN_PIPELINE,
	SHADER_OBJECT_COMPUTE_PIPELINE,
};

struct TestParams {
	TestType	testType;
};

struct StageTestParams {
	bool vertShader;
	bool tessShader;
	bool geomShader;
	bool fragShader;
};

class ShaderObjectPipelineInteractionInstance : public vkt::TestInstance
{
public:
							ShaderObjectPipelineInteractionInstance		(Context& context, const TestParams& params)
																		: vkt::TestInstance	(context)
																		, m_params			(params)
																		{}
	virtual					~ShaderObjectPipelineInteractionInstance	(void) {}

	tcu::TestStatus			iterate										(void) override;
private:
	bool					verifyImage									(de::MovePtr<vk::BufferWithMemory>& outputBuffer, deUint32 drawCount);
	deUint32				getDrawCount								(void);
	TestParams				m_params;

	const vk::VkFormat		colorAttachmentFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const vk::VkRect2D		renderArea					= { {0u, 0u, }, {32u, 32u, } };
};

deUint32 ShaderObjectPipelineInteractionInstance::getDrawCount (void)
{
	switch (m_params.testType) {
		case SHADER_OBJECT:
			return 1;
		case MAX_PIPELINE:
			return 1;
		case MAX_PIPELINE_SHADER_OBJECT_MAX_PIPELINE:
			return 3;
		case SHADER_OBJECT_MAX_PIPELINE_SHADER_OBJECT:
			return 3;
		case MIN_PIPELINE_SHADER_OBJECT:
			return 2;
		case RENDER_PASS_PIPELINE_SHADER_OBJECT:
			return 1;
		case RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN:
			return 1;
		case SHADER_OBJECT_MIN_PIPELINE:
			return 2;
		case COMPUTE_SHADER_OBJECT_MIN_PIPELINE:
			return 1;
		case SHADER_OBJECT_COMPUTE_PIPELINE:
			return 1;
	}
	return 0;
}

bool extensionEnabled (const std::vector<std::string>& deviceExtensions, const std::string& ext)
{
	return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) != deviceExtensions.end();
}

tcu::TestStatus ShaderObjectPipelineInteractionInstance::iterate (void)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&								alloc						= m_context.getDefaultAllocator();
	const auto							deviceExtensions			= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const bool							taskSupported				= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported				= m_context.getMeshShaderFeatures().meshShader;

	const auto							subresourceRange			= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const vk::VkImageCreateInfo			createInfo					=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType
		DE_NULL,																		// const void*				pNext
		0u,																				// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,															// VkImageType				imageType
		colorAttachmentFormat,															// VkFormat					format
		{ renderArea.extent.width, renderArea.extent.height, 1 },						// VkExtent3D				extent
		1u,																				// uint32_t					mipLevels
		1u,																				// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode
		0,																				// uint32_t					queueFamilyIndexCount
		DE_NULL,																		// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>	image						= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView					= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);

	const vk::VkDeviceSize				colorOutputBufferSize		= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const vk::VkCommandPoolCreateInfo	cmdPoolInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,										// queuefamilyindex
	};

	const vk::Move<vk::VkCommandPool>	cmdPool					(createCommandPool(vk, device, &cmdPoolInfo));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const vk::Move<vk::VkCommandBuffer>	copyCmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const auto					pipelineLayout			= makePipelineLayout(vk, device);
	const auto					computePipelineLayout	= makePipelineLayout(vk, device, descriptorSetLayout.get());

	const auto&					binaries			= m_context.getBinaryCollection();
	const auto&					vert1				= binaries.get("vert1");
	const auto&					vert2				= binaries.get("vert2");
	const auto&					vert3				= binaries.get("vert3");
	const auto&					tesc				= binaries.get("tesc");
	const auto&					tese				= binaries.get("tese");
	const auto&					geom				= binaries.get("geom");
	const auto&					frag1				= binaries.get("frag1");
	const auto&					frag2				= binaries.get("frag2");
	const auto&					frag3				= binaries.get("frag3");
	const auto&					comp				= binaries.get("comp");

	// Todo
	vk::VkDescriptorSetLayout	layout				= descriptorSetLayout.get();

	vk::VkShaderCreateInfoEXT	vertCreateInfo1		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, vert1, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	vertCreateInfo2		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, vert2, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	vertCreateInfo3		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, vert3, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	tescCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tesc, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	teseCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tese, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	geomCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, geom, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	fragCreateInfo1		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, frag1, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	fragCreateInfo2		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, frag2, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	fragCreateInfo3		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, frag3, tessellationSupported, geometrySupported);
	vk::VkShaderCreateInfoEXT	compCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_COMPUTE_BIT, comp, tessellationSupported, geometrySupported, &layout);

	vk::Move<vk::VkShaderEXT>	vertShader1			= vk::createShader(vk, device, vertCreateInfo1);
	vk::Move<vk::VkShaderEXT>	vertShader2			= vk::createShader(vk, device, vertCreateInfo2);
	vk::Move<vk::VkShaderEXT>	vertShader3			= vk::createShader(vk, device, vertCreateInfo3);
	vk::Move<vk::VkShaderEXT>	tescShader			= vk::createShader(vk, device, tescCreateInfo);
	vk::Move<vk::VkShaderEXT>	teseShader			= vk::createShader(vk, device, teseCreateInfo);
	vk::Move<vk::VkShaderEXT>	geomShader			= vk::createShader(vk, device, geomCreateInfo);
	vk::Move<vk::VkShaderEXT>	fragShader1			= vk::createShader(vk, device, fragCreateInfo1);
	vk::Move<vk::VkShaderEXT>	fragShader2			= vk::createShader(vk, device, fragCreateInfo2);
	vk::Move<vk::VkShaderEXT>	fragShader3			= vk::createShader(vk, device, fragCreateInfo3);
	vk::Move<vk::VkShaderEXT>	compShader			= vk::createShader(vk, device, compCreateInfo);

	const auto					vertShaderModule1	= createShaderModule(vk, device, vert1);
	const auto					vertShaderModule2	= createShaderModule(vk, device, vert2);
	const auto					vertShaderModule3	= createShaderModule(vk, device, vert3);
	const auto					tescShaderModule	= createShaderModule(vk, device, tesc);
	const auto					teseShaderModule	= createShaderModule(vk, device, tese);
	const auto					geomShaderModule	= createShaderModule(vk, device, geom);
	const auto					fragShaderModule1	= createShaderModule(vk, device, frag1);
	const auto					fragShaderModule2	= createShaderModule(vk, device, frag2);
	const auto					fragShaderModule3	= createShaderModule(vk, device, frag3);
	const auto					compShaderModule	= createShaderModule(vk, device, comp);

	const auto					renderPass			= vk::makeRenderPass(vk, device, colorAttachmentFormat, vk::VK_FORMAT_UNDEFINED, vk::VK_ATTACHMENT_LOAD_OP_CLEAR, vk::VK_IMAGE_LAYOUT_GENERAL);
	const auto					framebuffer			= vk::makeFramebuffer(vk, device, *renderPass, 1u, &*imageView, renderArea.extent.width, renderArea.extent.height);

	const vk::VkPipelineVertexInputStateCreateInfo		vertexInputStateParams			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		0u,																// deUint32									vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,																// deUint32									vertexAttributeDescriptionCount;
		DE_NULL															// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const vk::VkPipelineTessellationStateCreateInfo		tessStateCreateInfo				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		DE_NULL,														//	const void*								pNext;
		0u,																//	VkPipelineTessellationStateCreateFlags	flags;
		4u,																//	uint32_t								patchControlPoints;
	};

	const vk::VkPipelineInputAssemblyStateCreateInfo	pipelineInputAssemblyStateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,															// const void*                                 pNext;
		(vk::VkPipelineInputAssemblyStateCreateFlags)0u,					// VkPipelineInputAssemblyStateCreateFlags     flags;
		vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,								// VkPrimitiveTopology                         topology;
		VK_FALSE,															// VkBool32                                    primitiveRestartEnable;
	};

	vk::VkPipelineRenderingCreateInfo					pipelineRenderingCreateInfo		=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,	// VkStructureType	sType
		DE_NULL,												// const void*		pNext
		0u,														// uint32_t			viewMask
		1u,														// uint32_t			colorAttachmentCount
		&colorAttachmentFormat,									// const VkFormat*	pColorAttachmentFormats
		vk::VK_FORMAT_UNDEFINED,								// VkFormat			depthAttachmentFormat
		vk::VK_FORMAT_UNDEFINED,								// VkFormat			stencilAttachmentFormat
	};

	const vk::VkViewport								viewport						= vk::makeViewport(renderArea.extent);
	const vk::VkRect2D									scissor							= vk::makeRect2D(renderArea.extent);

	bool createDynamicPipeline = m_params.testType != MIN_PIPELINE_SHADER_OBJECT && m_params.testType != SHADER_OBJECT_MIN_PIPELINE && m_params.testType != COMPUTE_SHADER_OBJECT_MIN_PIPELINE && m_params.testType != RENDER_PASS_PIPELINE_SHADER_OBJECT && m_params.testType != RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN;

	const vk::VkPipelineViewportStateCreateInfo			viewportStateCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,													// const void*                                 pNext
		(vk::VkPipelineViewportStateCreateFlags)0u,					// VkPipelineViewportStateCreateFlags          flags
		createDynamicPipeline ? 0u : 1u,							// deUint32                                    viewportCount
		&viewport,													// const VkViewport*                           pViewports
		createDynamicPipeline ? 0u : 1u,							// deUint32                                    scissorCount
		&scissor,													// const VkRect2D*                             pScissors
	};

	const auto& edsFeatures		= m_context.getExtendedDynamicStateFeaturesEXT();
	const auto& eds2Features	= m_context.getExtendedDynamicState2FeaturesEXT();
	const auto& eds3Features	= m_context.getExtendedDynamicState3FeaturesEXT();
	const auto& viFeatures		= m_context.getVertexInputDynamicStateFeaturesEXT();

	std::vector<vk::VkDynamicState> dynamicStates = {
		vk::VK_DYNAMIC_STATE_LINE_WIDTH,
		vk::VK_DYNAMIC_STATE_DEPTH_BIAS,
		vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS,
		vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS,
		vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
		vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE,
	};

	if (edsFeatures.extendedDynamicState)
	{
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CULL_MODE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRONT_FACE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_OP_EXT);
	}
	else
	{
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR);
	}
	if (eds2Features.extendedDynamicState2)
	{
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
	}
	if (eds2Features.extendedDynamicState2LogicOp)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT);
	if (eds2Features.extendedDynamicState2PatchControlPoints)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT);

	if (eds3Features.extendedDynamicState3TessellationDomainOrigin)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT);
	if (eds3Features.extendedDynamicState3DepthClampEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3PolygonMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
	if (eds3Features.extendedDynamicState3RasterizationSamples)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
	if (eds3Features.extendedDynamicState3SampleMask)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
	if (eds3Features.extendedDynamicState3AlphaToCoverageEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3AlphaToOneEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3LogicOpEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3ColorBlendEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3ColorBlendEquation)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
	if (eds3Features.extendedDynamicState3ColorWriteMask)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
	if (viFeatures.vertexInputDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

	if (extensionEnabled(deviceExtensions, "VK_EXT_transform_feedback") && eds3Features.extendedDynamicState3RasterizationStream)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_blend_operation_advanced") && eds3Features.extendedDynamicState3ColorBlendAdvanced)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_conservative_rasterization") && eds3Features.extendedDynamicState3ConservativeRasterizationMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples") && eds3Features.extendedDynamicState3CoverageModulationMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples") && eds3Features.extendedDynamicState3CoverageModulationTableEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples") && eds3Features.extendedDynamicState3CoverageModulationTable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_coverage_reduction_mode") && eds3Features.extendedDynamicState3CoverageReductionMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_fragment_coverage_to_color") && eds3Features.extendedDynamicState3CoverageToColorEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_fragment_coverage_to_color") && eds3Features.extendedDynamicState3CoverageToColorLocation)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV);
	if (extensionEnabled(deviceExtensions, "VK_EXT_depth_clip_enable") && eds3Features.extendedDynamicState3DepthClipEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_depth_clip_control") && eds3Features.extendedDynamicState3DepthClipNegativeOneToOne)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_color_write_enable"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_conservative_rasterization") && eds3Features.extendedDynamicState3ExtraPrimitiveOverestimationSize)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization") && eds3Features.extendedDynamicState3LineRasterizationMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization") && eds3Features.extendedDynamicState3LineStippleEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_provoking_vertex") && eds3Features.extendedDynamicState3ProvokingVertexMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_KHR_fragment_shading_rate"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
	if (extensionEnabled(deviceExtensions, "VK_NV_representative_fragment_test") && eds3Features.extendedDynamicState3RepresentativeFragmentTestEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_EXT_sample_locations") && eds3Features.extendedDynamicState3SampleLocationsEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_sample_locations"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);
	if (extensionEnabled(deviceExtensions, "VK_NV_shading_rate_image") && eds3Features.extendedDynamicState3ShadingRateImageEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_shading_rate_image"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_shading_rate_image"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_viewport_swizzle") && eds3Features.extendedDynamicState3ViewportSwizzle)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_clip_space_w_scaling") && eds3Features.extendedDynamicState3ViewportWScalingEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_clip_space_w_scaling"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_scissor_exclusive"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT);

	const vk::VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		DE_NULL,													//	const void*							pNext;
		(vk::VkPipelineDynamicStateCreateFlags)0u,					//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		dynamicStates.data(),										//	const VkDynamicState*				pDynamicStates;
	};
	const vk::VkPipelineDynamicStateCreateInfo* pipelineDynamicState = (createDynamicPipeline) ? &dynamicStateCreateInfo : DE_NULL;

	const vk::VkDeviceSize					bufferSizeBytes = sizeof(deUint32) * 4;

	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory outputBuffer(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const vk::VkDescriptorBufferInfo		descriptorInfo	= vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	vk::VkPipelineRenderingCreateInfo*		pPipelineRenderingCreateInfo	= &pipelineRenderingCreateInfo;
	vk::VkRenderPass						renderPassHandle				= VK_NULL_HANDLE;
	if (m_params.testType == RENDER_PASS_PIPELINE_SHADER_OBJECT || m_params.testType == RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN)
	{
		pPipelineRenderingCreateInfo = DE_NULL;
		renderPassHandle = *renderPass;
	}

	const auto					pipeline1				= makeGraphicsPipeline(vk, device, pipelineLayout.get(), vertShaderModule1.get(), tescShaderModule.get(), teseShaderModule.get(), geomShaderModule.get(), fragShaderModule1.get(), renderPassHandle, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, pipelineDynamicState, pPipelineRenderingCreateInfo);
	const auto					pipeline2				= makeGraphicsPipeline(vk, device, pipelineLayout.get(), vertShaderModule2.get(), tescShaderModule.get(), teseShaderModule.get(), geomShaderModule.get(), fragShaderModule2.get(), renderPassHandle, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, pipelineDynamicState, pPipelineRenderingCreateInfo);
	const auto					pipeline3				= makeGraphicsPipeline(vk, device, pipelineLayout.get(), vertShaderModule3.get(), tescShaderModule.get(), teseShaderModule.get(), geomShaderModule.get(), fragShaderModule3.get(), renderPassHandle, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, pipelineDynamicState, pPipelineRenderingCreateInfo);
	const auto					computePipeline			= vk::makeComputePipeline(vk, device, computePipelineLayout.get(), compShaderModule.get());

	const vk::VkClearValue		clearValue				= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	vk::VkImageMemoryBarrier	initialBarrier			= vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);

	const vk::VkDeviceSize				bufferSize		= 64;
	de::MovePtr<vk::BufferWithMemory>	buffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible));

	vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, DE_NULL,
		0, DE_NULL, 1, &initialBarrier);

	if (m_params.testType == RENDER_PASS_PIPELINE_SHADER_OBJECT)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
	}

	if (m_params.testType != COMPUTE_SHADER_OBJECT_MIN_PIPELINE && m_params.testType != SHADER_OBJECT_COMPUTE_PIPELINE)
		vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false, !m_context.getExtendedDynamicStateFeaturesEXT().extendedDynamicState);
	vk::bindNullTaskMeshShaders(vk, *cmdBuffer, m_context.getMeshShaderFeaturesEXT());

	vk::VkDeviceSize offset = 0u;
	vk::VkDeviceSize stride = 16u;
	vk.cmdBindVertexBuffers2(*cmdBuffer, 0u, 1u, &**buffer, &offset, &bufferSize, &stride);

	if (m_params.testType == SHADER_OBJECT)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == MAX_PIPELINE)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == MAX_PIPELINE_SHADER_OBJECT_MAX_PIPELINE)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader2, *tescShader, *teseShader, *geomShader, *fragShader2, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline3);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == SHADER_OBJECT_MAX_PIPELINE_SHADER_OBJECT)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline2);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader3, *tescShader, *teseShader, *geomShader, *fragShader3, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == MIN_PIPELINE_SHADER_OBJECT)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader2, *tescShader, *teseShader, *geomShader, *fragShader2, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == RENDER_PASS_PIPELINE_SHADER_OBJECT)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == SHADER_OBJECT_MIN_PIPELINE)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline2);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == COMPUTE_SHADER_OBJECT_MIN_PIPELINE)
	{
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vk::VkShaderStageFlagBits stages[] = { vk::VK_SHADER_STAGE_COMPUTE_BIT  };
		vk.cmdBindShadersEXT(*cmdBuffer, 1, stages, &*compShader);
		vk.cmdDispatch(*cmdBuffer, 4, 1, 1);

		vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::endRendering(vk, *cmdBuffer);
	}
	else if (m_params.testType == SHADER_OBJECT_COMPUTE_PIPELINE)
	{
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader1, *tescShader, *teseShader, *geomShader, *fragShader1, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::endRendering(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.cmdDispatch(*cmdBuffer, 4, 1, 1);
	}

	if (m_params.testType != COMPUTE_SHADER_OBJECT_MIN_PIPELINE && m_params.testType != SHADER_OBJECT_COMPUTE_PIPELINE)
		vk::endRendering(vk, *cmdBuffer);

	vk::endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	const vk::VkBufferImageCopy	copyRegion =
	{
		0u,																		// VkDeviceSize				bufferOffset;
		0u,																		// deUint32					bufferRowLength;
		0u,																		// deUint32					bufferImageHeight;
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,										// VkImageAspectFlags		aspect;
			0u,																	// deUint32					mipLevel;
			0u,																	// deUint32					baseArrayLayer;
			1u,																	// deUint32					layerCount;
		},																		// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },															// VkOffset3D				imageOffset;
		{renderArea.extent.width, renderArea.extent.height, 1}					// VkExtent3D				imageExtent;
	};

	vk::beginCommandBuffer(vk, *copyCmdBuffer, 0u);
	vk.cmdCopyImageToBuffer(*copyCmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);
	vk::endCommandBuffer(vk, *copyCmdBuffer);
	submitCommandsAndWait(vk, device, queue, copyCmdBuffer.get());

	if (!verifyImage(colorOutputBuffer, getDrawCount()))
		return tcu::TestStatus::fail("Fail");

	if (m_params.testType == COMPUTE_SHADER_OBJECT_MIN_PIPELINE || m_params.testType == SHADER_OBJECT_COMPUTE_PIPELINE)
	{
		const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
		invalidateAlloc(vk, device, outputBufferAllocation);

		const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

		for (deUint32 i = 0; i < 4; ++i)
		{
			if (bufferPtr[i] != i)
				return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

bool ShaderObjectPipelineInteractionInstance::verifyImage (de::MovePtr<vk::BufferWithMemory>& outputBuffer, deUint32 drawCount)
{
	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM), renderArea.extent.width, renderArea.extent.height, 1, (const void*)outputBuffer->getAllocation().getHostPtr());

	const tcu::Vec4			red			= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4			green		= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4			blue		= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
	const deInt32			width		= resultBuffer.getWidth();
	const deInt32			height		= resultBuffer.getHeight();

	for (deInt32 j = 0; j < height; ++j)
	{
		for (deInt32 i = 0; i < width; ++i)
		{
			const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();
			if (i < width / 2 && j < height / 2 && drawCount > 0)
			{
				if (color != red)
					return false;
			}
			else if (i >= width / 2 && j < height / 2 && drawCount > 1)
			{
				if (color != green)
					return false;
			}
			else if (i < width / 2 && j >= height / 2 && drawCount > 2)
			{
				if (color != blue)
					return false;
			}
		}
	}

	return true;
}

class ShaderObjectPipelineInteractionCase : public vkt::TestCase
{
public:
					ShaderObjectPipelineInteractionCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
															: vkt::TestCase		(testCtx, name, description)
															, m_params			(params)
															{}
	virtual			~ShaderObjectPipelineInteractionCase	(void) {}

	void			checkSupport				(vkt::Context& context) const override;
	virtual void	initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override { return new ShaderObjectPipelineInteractionInstance(context, m_params); }
private:
	TestParams m_params;
};

void ShaderObjectPipelineInteractionCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vert1, vert2, vert3;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag1, frag2, frag3;
	std::stringstream comp;

	vert1
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos * 0.5f - vec2(0.5f, 0.5f), 0.0f, 1.0f);\n"
		<< "}\n";
	vert2
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos * 0.5f - vec2(0.0f, 0.5f), 0.0f, 1.0f);\n"
		<< "}\n";
	vert3
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos * 0.5f - vec2(0.5f, 0.0f), 0.0f, 1.0f);\n"
		<< "}\n";

	tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n"
		<< "		gl_TessLevelInner[0] = 1.0;\n"
		<< "		gl_TessLevelInner[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[0] = 1.0;\n"
		<< "		gl_TessLevelOuter[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[2] = 1.0;\n"
		<< "		gl_TessLevelOuter[3] = 1.0;\n"
		<< "	}\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";

	tese
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads, equal_spacing) in;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 2.0f;\n"
		<< "}\n";

	geom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.y *= 2.0f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.y *= 2.0f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.y *= 2.0f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "}\n";

	frag1
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "}\n";
	frag2
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
		<< "}\n";
	frag3
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
		<< "}\n";

	comp
		<< "#version 450\n"
		<< "layout(local_size_x=16, local_size_x=1, local_size_x=1) in;\n"
		<< "layout(binding = 0) buffer Output {\n"
		<< "    uint values[16];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "    buffer_out.values[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
		<< "}\n";

	programCollection.glslSources.add("vert1") << glu::VertexSource(vert1.str());
	programCollection.glslSources.add("vert2") << glu::VertexSource(vert2.str());
	programCollection.glslSources.add("vert3") << glu::VertexSource(vert3.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag1") << glu::FragmentSource(frag1.str());
	programCollection.glslSources.add("frag2") << glu::FragmentSource(frag2.str());
	programCollection.glslSources.add("frag3") << glu::FragmentSource(frag3.str());
	programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void ShaderObjectPipelineInteractionCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");

	context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
	context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}


class ShaderObjectStageBindingInstance : public vkt::TestInstance
{
public:
							ShaderObjectStageBindingInstance			(Context& context, const StageTestParams& params)
																		: vkt::TestInstance	(context)
																		, m_params			(params)
																		{}
	virtual					~ShaderObjectStageBindingInstance			(void) {}

	tcu::TestStatus			iterate										(void) override;
private:
	bool					verifyImage									(de::MovePtr<vk::BufferWithMemory>& outputBuffer);
	StageTestParams			m_params;

	const vk::VkFormat		colorAttachmentFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const vk::VkRect2D		renderArea				= { {0u, 0u, }, {32u, 32u, } };
};

tcu::TestStatus ShaderObjectStageBindingInstance::iterate (void)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&								alloc						= m_context.getDefaultAllocator();
	const auto							deviceExtensions			= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const bool							taskSupported				= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported				= m_context.getMeshShaderFeatures().meshShader;

	const auto							subresourceRange			= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const vk::VkImageCreateInfo			createInfo					=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType
		DE_NULL,																		// const void*				pNext
		0u,																				// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,															// VkImageType				imageType
		colorAttachmentFormat,															// VkFormat					format
		{ renderArea.extent.width, renderArea.extent.height, 1 },						// VkExtent3D				extent
		1u,																				// uint32_t					mipLevels
		1u,																				// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode
		0,																				// uint32_t					queueFamilyIndexCount
		DE_NULL,																		// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>	image						= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView					= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);

	const vk::VkDeviceSize				colorOutputBufferSize		= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const vk::VkCommandPoolCreateInfo	cmdPoolInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,										// queuefamilyindex
	};

	const vk::Move<vk::VkCommandPool>	cmdPool					(createCommandPool(vk, device, &cmdPoolInfo));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const vk::Move<vk::VkCommandBuffer>	copyCmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const auto					topology			= m_params.tessShader ? vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const auto					pipelineLayout		= makePipelineLayout(vk, device, descriptorSetLayout.get());
	const auto					emptyPipelineLayout	= makePipelineLayout(vk, device);

	const auto&					binaries			= m_context.getBinaryCollection();
	const auto&					vert				= binaries.get("vert");
	const auto&					tesc				= binaries.get("tesc");
	const auto&					tese				= binaries.get("tese");
	const auto&					geom				= binaries.get("geom");
	const auto&					frag				= binaries.get("frag");

	const auto&					pipeline_vert		= binaries.get("pipeline_vert");
	const auto&					pipeline_tesc		= binaries.get("pipeline_tesc");
	const auto&					pipeline_tese		= binaries.get("pipeline_tese");
	const auto&					pipeline_geom		= binaries.get("pipeline_geom");
	const auto&					pipeline_frag		= binaries.get("pipeline_frag");

	vk::VkDescriptorSetLayout	layout				= descriptorSetLayout.get();

	vk::VkShaderCreateInfoEXT	vertCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, vert, tessellationSupported, geometrySupported, &layout);
	vk::VkShaderCreateInfoEXT	tescCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tesc, tessellationSupported, geometrySupported, &layout);
	vk::VkShaderCreateInfoEXT	teseCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tese, tessellationSupported, geometrySupported, &layout);
	vk::VkShaderCreateInfoEXT	geomCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, geom, tessellationSupported, geometrySupported, &layout);
	vk::VkShaderCreateInfoEXT	fragCreateInfo		= vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, frag, tessellationSupported, geometrySupported, &layout);

	vk::Move<vk::VkShaderEXT>	vertShader			= vk::createShader(vk, device, vertCreateInfo);
	vk::Move<vk::VkShaderEXT>	tescShader;
	vk::Move<vk::VkShaderEXT>	teseShader;
	vk::Move<vk::VkShaderEXT>	geomShader;
	vk::Move<vk::VkShaderEXT>	fragShader			= vk::createShader(vk, device, fragCreateInfo);

	vk::Move<vk::VkShaderModule>	vertShaderModule = createShaderModule(vk, device, pipeline_vert);
	vk::Move<vk::VkShaderModule>	tescShaderModule;
	vk::Move<vk::VkShaderModule>	teseShaderModule;
	vk::Move<vk::VkShaderModule>	geomShaderModule;
	vk::Move<vk::VkShaderModule>	fragShaderModule = createShaderModule(vk, device, pipeline_frag);

	if (m_params.tessShader)
	{
		tescShader = vk::createShader(vk, device, tescCreateInfo);
		teseShader = vk::createShader(vk, device, teseCreateInfo);

		tescShaderModule = createShaderModule(vk, device, pipeline_tesc);
		teseShaderModule = createShaderModule(vk, device, pipeline_tese);
	}
	if (m_params.geomShader)
	{
		geomShader = vk::createShader(vk, device, geomCreateInfo);

		geomShaderModule = createShaderModule(vk, device, pipeline_geom);
	}

	const vk::VkPipelineVertexInputStateCreateInfo		vertexInputStateParams =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		0u,																// deUint32									vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,																// deUint32									vertexAttributeDescriptionCount;
		DE_NULL															// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const vk::VkPipelineTessellationStateCreateInfo		tessStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		DE_NULL,														//	const void*								pNext;
		0u,																//	VkPipelineTessellationStateCreateFlags	flags;
		4u,																//	uint32_t								patchControlPoints;
	};

	const vk::VkPipelineInputAssemblyStateCreateInfo	pipelineInputAssemblyStateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,															// const void*                                 pNext;
		(vk::VkPipelineInputAssemblyStateCreateFlags)0u,					// VkPipelineInputAssemblyStateCreateFlags     flags;
		topology,															// VkPrimitiveTopology                         topology;
		VK_FALSE,															// VkBool32                                    primitiveRestartEnable;
	};

	vk::VkPipelineRenderingCreateInfo					pipelineRenderingCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,	// VkStructureType	sType
		DE_NULL,												// const void*		pNext
		0u,														// uint32_t			viewMask
		1u,														// uint32_t			colorAttachmentCount
		&colorAttachmentFormat,									// const VkFormat*	pColorAttachmentFormats
		vk::VK_FORMAT_UNDEFINED,								// VkFormat			depthAttachmentFormat
		vk::VK_FORMAT_UNDEFINED,								// VkFormat			stencilAttachmentFormat
	};

	const vk::VkViewport								viewport				= vk::makeViewport((float)renderArea.extent.width, 0.0f, (float)renderArea.extent.width, (float)renderArea.extent.height, 0.0f, 1.0f);
	const vk::VkRect2D									scissor					= vk::makeRect2D(renderArea.extent);

	const vk::VkPipelineViewportStateCreateInfo			viewportStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,													// const void*                                 pNext
		(vk::VkPipelineViewportStateCreateFlags)0u,					// VkPipelineViewportStateCreateFlags          flags
		1u,															// deUint32                                    viewportCount
		&viewport,													// const VkViewport*                           pViewports
		1u,															// deUint32                                    scissorCount
		&scissor													// const VkRect2D*                             pScissors
	};

	const auto								pipeline		= makeGraphicsPipeline(vk, device, emptyPipelineLayout.get(), vertShaderModule.get(), tescShaderModule.get(), teseShaderModule.get(), geomShaderModule.get(), fragShaderModule.get(), VK_NULL_HANDLE, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, DE_NULL, &pipelineRenderingCreateInfo);


	const vk::VkDeviceSize					bufferSizeBytes = sizeof(deUint32) * 4;

	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory				outputBuffer	(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const vk::VkDescriptorBufferInfo		descriptorInfo	= vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const vk::VkClearValue					clearValue		= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	vk::VkImageMemoryBarrier				initialBarrier	= vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);

	vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, DE_NULL,
		0, DE_NULL, 1, &initialBarrier);

	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, topology, false, !m_context.getExtendedDynamicStateFeaturesEXT().extendedDynamicState);

	vk::bindGraphicsShaders(vk, *cmdBuffer,
		(m_params.vertShader) ? *vertShader : VK_NULL_HANDLE,
		(m_params.tessShader) ? *tescShader : VK_NULL_HANDLE,
		(m_params.tessShader) ? *teseShader : VK_NULL_HANDLE,
		(m_params.geomShader) ? *geomShader : VK_NULL_HANDLE,
		(m_params.fragShader) ? *fragShader : VK_NULL_HANDLE,
		taskSupported,
		meshSupported);

	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

	vk::endRendering(vk, *cmdBuffer);

	vk::endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	if (m_params.fragShader)
	{
		const vk::VkBufferImageCopy	copyRegion =
		{
			0u,																		// VkDeviceSize				bufferOffset;
			0u,																		// deUint32					bufferRowLength;
			0u,																		// deUint32					bufferImageHeight;
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,										// VkImageAspectFlags		aspect;
				0u,																	// deUint32					mipLevel;
				0u,																	// deUint32					baseArrayLayer;
				1u,																	// deUint32					layerCount;
			},																		// VkImageSubresourceLayers	imageSubresource;
			{ 0, 0, 0 },															// VkOffset3D				imageOffset;
			{renderArea.extent.width, renderArea.extent.height, 1}					// VkExtent3D				imageExtent;
		};

		vk::beginCommandBuffer(vk, *copyCmdBuffer, 0u);
		vk.cmdCopyImageToBuffer(*copyCmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);
		vk::endCommandBuffer(vk, *copyCmdBuffer);
		submitCommandsAndWait(vk, device, queue, copyCmdBuffer.get());

		if (!verifyImage(colorOutputBuffer))
			return tcu::TestStatus::fail("Fail");
	}

	const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

	if (m_params.vertShader && bufferPtr[0] != 1u)
		return tcu::TestStatus::fail("Fail");
	if (m_params.tessShader && bufferPtr[1] != 2u)
		return tcu::TestStatus::fail("Fail");
	if (m_params.geomShader && bufferPtr[2] != 3u)
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

bool ShaderObjectStageBindingInstance::verifyImage (de::MovePtr<vk::BufferWithMemory>& outputBuffer)
{
	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM), renderArea.extent.width, renderArea.extent.height, 1, (const void*)outputBuffer->getAllocation().getHostPtr());

	const tcu::Vec4			black		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4			white		= tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	const deInt32			width		= resultBuffer.getWidth();
	const deInt32			height		= resultBuffer.getHeight();

	const deInt32			xOffset		= m_params.tessShader ? 4 : 8;
	const deInt32			yOffset		= m_params.geomShader ? 4 : 8;

	for (deInt32 j = 0; j < height; ++j)
	{
		for (deInt32 i = 0; i < width; ++i)
		{
			const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();
			if (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset)
			{
				if (color != white)
					return false;
			}
			else
			{
				if (color != black)
					return false;
			}
		}
	}

	return true;
}

class ShaderObjectStageBindingCase : public vkt::TestCase
{
public:
					ShaderObjectStageBindingCase			(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const StageTestParams& params)
															: vkt::TestCase		(testCtx, name, description)
															, m_params			(params)
															{}
	virtual			~ShaderObjectStageBindingCase			(void) {}

	void			checkSupport							(vkt::Context& context) const override;
	virtual void	initPrograms							(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance							(Context& context) const override { return new ShaderObjectStageBindingInstance(context, m_params); }
private:
	StageTestParams m_params;
};

void ShaderObjectStageBindingCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");

	if (m_params.tessShader)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params.geomShader)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectStageBindingCase::initPrograms(vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;

	std::stringstream pipeline_vert;
	std::stringstream pipeline_geom;
	std::stringstream pipeline_tesc;
	std::stringstream pipeline_tese;
	std::stringstream pipeline_frag;

	vert
		<< "#version 450\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n"
		<< "	if (gl_VertexIndex == 0u)\n"
		<< "		buffer_out.values[0] = 1u;\n"
		<< "}\n";

	tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n"
		<< "		gl_TessLevelInner[0] = 1.0;\n"
		<< "		gl_TessLevelInner[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[0] = 1.0;\n"
		<< "		gl_TessLevelOuter[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[2] = 1.0;\n"
		<< "		gl_TessLevelOuter[3] = 1.0;\n"
		<< "		buffer_out.values[1] = 2u;\n"
		<< "	}\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";

	tese
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads, equal_spacing) in;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 1.5f;\n"
		<< "}\n";

	geom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "    if (gl_InvocationID == 0u)\n"
		<< "		buffer_out.values[2] = 3u;\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";

	pipeline_vert
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n"
		<< "}\n";

	pipeline_tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n"
		<< "		gl_TessLevelInner[0] = 1.0;\n"
		<< "		gl_TessLevelInner[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[0] = 1.0;\n"
		<< "		gl_TessLevelOuter[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[2] = 1.0;\n"
		<< "		gl_TessLevelOuter[3] = 1.0;\n"
		<< "	}\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";

	pipeline_tese
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads, equal_spacing) in;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 0.5f;\n"
		<< "	gl_Position.y *= 0.5f;\n"
		<< "}\n";

	pipeline_geom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.x += 0.25f;\n"
		<< "	gl_Position.y += 0.25f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.x += 0.25f;\n"
		<< "	gl_Position.y += 0.25f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.x += 0.25f;\n"
		<< "	gl_Position.y += 0.25f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "}\n";

	pipeline_frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	programCollection.glslSources.add("pipeline_vert") << glu::VertexSource(pipeline_vert.str());
	programCollection.glslSources.add("pipeline_tesc") << glu::TessellationControlSource(pipeline_tesc.str());
	programCollection.glslSources.add("pipeline_tese") << glu::TessellationEvaluationSource(pipeline_tese.str());
	programCollection.glslSources.add("pipeline_geom") << glu::GeometrySource(pipeline_geom.str());
	programCollection.glslSources.add("pipeline_frag") << glu::FragmentSource(pipeline_frag.str());
}

}

tcu::TestCaseGroup* createShaderObjectPipelineInteractionTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineInteractionGroup(new tcu::TestCaseGroup(testCtx, "pipeline_interaction", ""));

	const struct
	{
		TestType	testType;
		const char* name;
	} tests[] =
	{
		{ SHADER_OBJECT,									"shader_object"										},
		{ MAX_PIPELINE,										"max_pipeline"										},
		{ MAX_PIPELINE_SHADER_OBJECT_MAX_PIPELINE,			"max_pipeline_shader_object_max_pipeline"			},
		{ SHADER_OBJECT_MAX_PIPELINE_SHADER_OBJECT,			"shader_object_max_pipeline_shader_object"			},
		{ MIN_PIPELINE_SHADER_OBJECT,						"min_pipeline_shader_object"						},
		{ SHADER_OBJECT_MIN_PIPELINE,						"shader_object_min_pipeline"						},
		{ RENDER_PASS_PIPELINE_SHADER_OBJECT,				"render_pass_pipeline_shader_object"				},
		{ RENDER_PASS_PIPELINE_SHADER_OBJECT_AFTER_BEGIN,	"render_pass_pipeline_shader_object_after_begin"	},
		{ COMPUTE_SHADER_OBJECT_MIN_PIPELINE,				"compute_shader_object_min_pipeline"				},
		{ SHADER_OBJECT_COMPUTE_PIPELINE,					"shader_object_compute_pipeline"					},
	};

	for (const auto& test : tests)
	{
		TestParams params;
		params.testType = test.testType;

		pipelineInteractionGroup->addChild(new ShaderObjectPipelineInteractionCase(testCtx, test.name, "", params));
	}

	const struct
	{
		StageTestParams	shaders;
		const char* name;
	} shaderBindTests[] =
	{
		{ { true, false, false, false },	"vert"	},
		{ { true, true, false, false },		"vert_tess"	},
		{ { true, false, true, false },		"vert_geom"	},
		{ { true, false, false, true },		"vert_frag"	},
		{ { true, true, true, false },		"vert_tess_geom"	},
		{ { true, true, false, true },		"vert_tess_frag"	},
		{ { true, false, true, true },		"vert_geom_frag"	},
		{ { true, true, true, true },		"vert_tess_geom_frag"	},
	};

	for (const auto& shaderBindTest : shaderBindTests)
	{
		pipelineInteractionGroup->addChild(new ShaderObjectStageBindingCase(testCtx, shaderBindTest.name, "", shaderBindTest.shaders));
	}

	return pipelineInteractionGroup.release();
}

} // ShaderObject
} // vkt
