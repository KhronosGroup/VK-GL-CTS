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
 * \brief Shader Object Performance Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectPerformanceTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkShaderObjectUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include <chrono>

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum TestType {
	DRAW_STATIC_PIPELINE,
	DRAW_DYNAMIC_PIPELINE,
	DRAW_LINKED_SHADERS,
	DRAW_BINARY,
	DRAW_BINARY_BIND,
};

enum BinaryType {
	BINARY_SHADER_CREATE,
	BINARY_MEMCPY,
};

enum DrawType {
	DRAW,
	DRAW_INDEXED,
	DRAW_INDEXED_INDIRECT,
	DRAW_INDEXED_INDIRECT_COUNT,
	DRAW_INDIRECT,
	DRAW_INDIRECT_COUNT,
};

enum DispatchType {
	DISPATCH,
	DISPATCH_BASE,
	DISPATCH_INDIRECT,
};

class ShaderObjectPerformanceInstance : public vkt::TestInstance
{
public:
								ShaderObjectPerformanceInstance		(Context& context, const DrawType drawType, const TestType& type)
																	: vkt::TestInstance	(context)
																	, m_drawType		(drawType)
																	, m_type			(type)
																	{}
	virtual						~ShaderObjectPerformanceInstance	(void) {}

	tcu::TestStatus				iterate								(void) override;
private:
	std::chrono::nanoseconds	draw								(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkBuffer indexBuffer, vk::VkBuffer indirectBuffer, vk::VkBuffer countBuffer) const;

	const DrawType	m_drawType;
	const TestType	m_type;
};

bool extensionEnabled(const std::vector<std::string>& deviceExtensions, const std::string& ext)
{
	return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) != deviceExtensions.end();
}

std::vector<vk::VkDynamicState> getDynamicStates (Context& context)
{
	const auto deviceExtensions		= vk::removeUnsupportedShaderObjectExtensions(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceExtensions());
	const auto& edsFeatures			= context.getExtendedDynamicStateFeaturesEXT();
	const auto& eds2Features		= context.getExtendedDynamicState2FeaturesEXT();
	const auto& eds3Features		= context.getExtendedDynamicState3FeaturesEXT();
	const auto& viFeatures			= context.getVertexInputDynamicStateFeaturesEXT();

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

	return dynamicStates;
}

vk::VkShaderEXT createShaderFromBinary (const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::Move<vk::VkShaderEXT>& shader, vk::VkShaderStageFlagBits stage)
{
	size_t						dataSize = 0;
	vk.getShaderBinaryDataEXT(device, *shader, &dataSize, DE_NULL);
	std::vector<deUint8>		data(dataSize);
	vk.getShaderBinaryDataEXT(device, *shader, &dataSize, data.data());

	const vk::VkShaderCreateInfoEXT binaryShaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		0u,												// VkShaderCreateFlagsEXT		flags;
		stage,											// VkShaderStageFlagBits		stage;
		0u,												// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_BINARY_EXT,				// VkShaderCodeTypeEXT			codeType;
		dataSize,										// size_t						codeSize;
		data.data(),									// const void*					pCode;
		"main",											// const char*					pName;
		0u,												// uint32_t						setLayoutCount;
		DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
		0u,												// uint32_t						pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
	};

	vk::VkShaderEXT binaryShader;
	vk.createShadersEXT(device, 1, &binaryShaderCreateInfo, DE_NULL, &binaryShader);
	return binaryShader;
}

std::chrono::nanoseconds ShaderObjectPerformanceInstance::draw (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkBuffer indexBuffer, vk::VkBuffer indirectBuffer, vk::VkBuffer countBuffer) const
{
	if (m_drawType == DRAW)
	{
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDraw(cmdBuffer, 4, 1, 0, 0);
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	else if (m_drawType == DRAW_INDEXED)
	{
		vk.cmdBindIndexBuffer(cmdBuffer, indexBuffer, 0u, vk::VK_INDEX_TYPE_UINT16);
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDrawIndexed(cmdBuffer, 4, 1, 0, 0, 0);
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	else if (m_drawType == DRAW_INDEXED_INDIRECT)
	{
		vk.cmdBindIndexBuffer(cmdBuffer, indexBuffer, 0u, vk::VK_INDEX_TYPE_UINT16);
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDrawIndexedIndirect(cmdBuffer, indirectBuffer, 0u, 1u, sizeof(vk::VkDrawIndexedIndirectCommand));
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	else if (m_drawType == DRAW_INDEXED_INDIRECT_COUNT)
	{
		vk.cmdBindIndexBuffer(cmdBuffer, indexBuffer, 0u, vk::VK_INDEX_TYPE_UINT16);
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDrawIndexedIndirectCount(cmdBuffer, indirectBuffer, 0u, countBuffer, 0u, 1u, sizeof(vk::VkDrawIndexedIndirectCommand));
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	else if (m_drawType == DRAW_INDIRECT)
	{
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDrawIndirect(cmdBuffer, indirectBuffer, 0u, 1u, sizeof(vk::VkDrawIndirectCommand));
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	else if (m_drawType == DRAW_INDIRECT_COUNT)
	{
		const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
		vk.cmdDrawIndirectCount(cmdBuffer, indirectBuffer, 0u, countBuffer, 0u, 1u, sizeof(vk::VkDrawIndirectCommand));
		return std::chrono::high_resolution_clock::now() - shaderObjectStart;
	}
	return std::chrono::nanoseconds(0);
}

tcu::TestStatus ShaderObjectPerformanceInstance::iterate (void)
{
	const vk::VkInstance				instance					= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver				(m_context.getPlatformInterface(), instance);
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

	vk::VkFormat						colorAttachmentFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const auto							subresourceRange			= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const vk::Move<vk::VkCommandPool>	cmdPool						(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer					(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkPrimitiveTopology		topology					= tessellationSupported ? vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const deUint32						geomIndex					= tessellationSupported ? 4u : 2u;

	const vk::VkImageCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		colorAttachmentFormat,						// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>	image					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView				= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
	const vk::VkRect2D					renderArea				= vk::makeRect2D(0, 0, 32, 32);

	const vk::VkDeviceSize				colorOutputBufferSize	= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const auto&		binaries	= m_context.getBinaryCollection();

	std::vector<vk::VkShaderCreateInfoEXT> createInfos =
	{
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported),
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), tessellationSupported, geometrySupported),
	};

	if (tessellationSupported)
	{
		createInfos.push_back(vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported));
		createInfos.push_back(vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"), tessellationSupported, geometrySupported));
	}
	if (geometrySupported)
	{
		createInfos.push_back(vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, binaries.get("geom"), tessellationSupported, geometrySupported));
	}

	if (tessellationSupported)
	{
		createInfos[0].nextStage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		createInfos[2].nextStage = vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		if (geometrySupported)
			createInfos[3].nextStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		else
			createInfos[3].nextStage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	else if (geometrySupported)
	{
		createInfos[0].nextStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		createInfos[geomIndex].nextStage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	else
	{
		createInfos[0].nextStage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	vk::Move<vk::VkShaderEXT>		vertShader		 = vk::createShader(vk, device, createInfos[0]);
	vk::Move<vk::VkShaderEXT>		fragShader		 = vk::createShader(vk, device, createInfos[1]);
	vk::Move<vk::VkShaderEXT>		tescShader;
	vk::Move<vk::VkShaderEXT>		teseShader;
	vk::Move<vk::VkShaderEXT>		geomShader;

	if (tessellationSupported)
	{
		tescShader = vk::createShader(vk, device, createInfos[2]);
		teseShader = vk::createShader(vk, device, createInfos[3]);
	}
	if (geometrySupported)
	{
		geomShader = vk::createShader(vk, device, createInfos[geomIndex]);
	}

	std::vector<vk::VkShaderEXT> refShaders;

	if (m_type == DRAW_LINKED_SHADERS)
	{
		refShaders.resize(5, VK_NULL_HANDLE);
		for (auto& info : createInfos)
			info.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;

		vk.createShadersEXT(device, (deUint32)createInfos.size(), createInfos.data(), DE_NULL, refShaders.data());
	}
	else if (m_type == DRAW_BINARY || m_type == DRAW_BINARY_BIND)
	{
		refShaders.resize(5, VK_NULL_HANDLE);
		refShaders[0] = createShaderFromBinary(vk, device, vertShader, vk::VK_SHADER_STAGE_VERTEX_BIT);
		refShaders[1] = createShaderFromBinary(vk, device, fragShader, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
		if (tessellationSupported)
		{
			refShaders[2] = createShaderFromBinary(vk, device, tescShader, vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
			refShaders[3] = createShaderFromBinary(vk, device, teseShader, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
		}
		if (geometrySupported)
		{
			refShaders[geomIndex] = createShaderFromBinary(vk, device, geomShader, vk::VK_SHADER_STAGE_GEOMETRY_BIT);
		}
	}

	vk::VkShaderEXT linkedShaders[5];
	vk.createShadersEXT(device, static_cast<uint32_t>(createInfos.size()), createInfos.data(), nullptr, linkedShaders);

	vk::Move<vk::VkShaderEXT>		linkedVertShader = vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(linkedShaders[0]), vk::Deleter<vk::VkShaderEXT>(vk, device, DE_NULL));
	vk::Move<vk::VkShaderEXT>		linkedFragShader = vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(linkedShaders[1]), vk::Deleter<vk::VkShaderEXT>(vk, device, DE_NULL));
	vk::Move<vk::VkShaderEXT>		linkedTescShader;
	vk::Move<vk::VkShaderEXT>		linkedTeseShader;
	vk::Move<vk::VkShaderEXT>		linkedGeomShader;
	if (tessellationSupported)
	{
		linkedTescShader = vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(linkedShaders[2]), vk::Deleter<vk::VkShaderEXT>(vk, device, DE_NULL));
		linkedTeseShader = vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(linkedShaders[3]), vk::Deleter<vk::VkShaderEXT>(vk, device, DE_NULL));
	}
	if (geometrySupported)
	{
		linkedGeomShader = vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(linkedShaders[geomIndex]), vk::Deleter<vk::VkShaderEXT>(vk, device, DE_NULL));
	}

	vk::Move<vk::VkShaderModule>		vertShaderModule = createShaderModule(vk, device, binaries.get("vert"));
	vk::Move<vk::VkShaderModule>		fragShaderModule = createShaderModule(vk, device, binaries.get("frag"));
	vk::Move<vk::VkShaderModule>		dummyVertShaderModule = createShaderModule(vk, device, binaries.get("dummyVert"));
	vk::Move<vk::VkShaderModule>		dummyFragShaderModule = createShaderModule(vk, device, binaries.get("dummyFrag"));
	vk::Move<vk::VkShaderModule>		tescShaderModule;
	vk::Move<vk::VkShaderModule>		teseShaderModule;
	vk::Move<vk::VkShaderModule>		geomShaderModule;
	if (tessellationSupported)
	{
		tescShaderModule = createShaderModule(vk, device, binaries.get("tesc"));
		teseShaderModule = createShaderModule(vk, device, binaries.get("tese"));
	}
	if (geometrySupported)
	{
		geomShaderModule = createShaderModule(vk, device, binaries.get("geom"));
	}

	const auto		emptyPipelineLayout = makePipelineLayout(vk, device);

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

	vk::VkPipelineInputAssemblyStateCreateInfo			pipelineInputAssemblyStateInfo =
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

	const vk::VkViewport								viewport	= vk::makeViewport((float)renderArea.extent.width, 0.0f, (float)renderArea.extent.width, (float)renderArea.extent.height, 0.0f, 1.0f);
	const vk::VkRect2D									scissor		= vk::makeRect2D(renderArea.extent);

	vk::VkPipelineViewportStateCreateInfo				viewportStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType
		DE_NULL,													// const void*                                 pNext
		(vk::VkPipelineViewportStateCreateFlags)0u,					// VkPipelineViewportStateCreateFlags          flags
		(m_type == DRAW_DYNAMIC_PIPELINE) ? 0u : 1u,				// deUint32                                    viewportCount
		&viewport,													// const VkViewport*                           pViewports
		(m_type == DRAW_DYNAMIC_PIPELINE) ? 0u : 1u,				// deUint32                                    scissorCount
		&scissor													// const VkRect2D*                             pScissors
	};

	const auto												dynamicStates			= getDynamicStates(m_context);

	const vk::VkPipelineDynamicStateCreateInfo				dynamicStateCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		DE_NULL,													//	const void*							pNext;
		(vk::VkPipelineDynamicStateCreateFlags)0u,					//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		dynamicStates.data(),										//	const VkDynamicState*				pDynamicStates;
	};

	const vk::VkPipelineDynamicStateCreateInfo*				pDynamicStateCreateInfo = (m_type == DRAW_DYNAMIC_PIPELINE) ? &dynamicStateCreateInfo : DE_NULL;

	const auto							pipeline			= makeGraphicsPipeline(vk, device, emptyPipelineLayout.get(), vertShaderModule.get(), tescShaderModule.get(), teseShaderModule.get(), geomShaderModule.get(), fragShaderModule.get(), VK_NULL_HANDLE, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, pDynamicStateCreateInfo, &pipelineRenderingCreateInfo);
	pipelineInputAssemblyStateInfo.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	viewportStateCreateInfo.viewportCount = 1u;
	viewportStateCreateInfo.scissorCount = 1u;
	const auto							dummyPipeline		= makeGraphicsPipeline(vk, device, emptyPipelineLayout.get(), dummyVertShaderModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, dummyFragShaderModule.get(), VK_NULL_HANDLE, 0u, &vertexInputStateParams, &pipelineInputAssemblyStateInfo, &tessStateCreateInfo, &viewportStateCreateInfo, DE_NULL, DE_NULL, DE_NULL, DE_NULL, DE_NULL, &pipelineRenderingCreateInfo);

	const vk::VkClearValue				clearValue			= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	vk::BufferWithMemory				indirectBuffer		(vk, device, alloc, vk::makeBufferCreateInfo(sizeof(vk::VkDrawIndirectCommand) + sizeof(vk::VkDrawIndexedIndirectCommand), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	if (m_drawType == DRAW_INDEXED_INDIRECT || m_drawType == DRAW_INDEXED_INDIRECT_COUNT)
	{
		vk::VkDrawIndexedIndirectCommand* indirectDataPtr = reinterpret_cast<vk::VkDrawIndexedIndirectCommand*>(indirectBuffer.getAllocation().getHostPtr());
		indirectDataPtr->indexCount = 4;
		indirectDataPtr->instanceCount = 1;
		indirectDataPtr->firstIndex = 0;
		indirectDataPtr->vertexOffset = 0;
		indirectDataPtr->firstInstance = 0;
	}
	else
	{
		vk::VkDrawIndirectCommand*		indirectDataPtr = reinterpret_cast<vk::VkDrawIndirectCommand*>(indirectBuffer.getAllocation().getHostPtr());
		indirectDataPtr->vertexCount = 4;
		indirectDataPtr->instanceCount = 1;
		indirectDataPtr->firstVertex = 0;
		indirectDataPtr->firstInstance = 0;
	}

	vk::BufferWithMemory				countBuffer			(vk, device, alloc, vk::makeBufferCreateInfo(sizeof(deUint32), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), vk::MemoryRequirement::HostVisible);
	deUint32*							countDataPtr		= reinterpret_cast<deUint32*>(countBuffer.getAllocation().getHostPtr());
	countDataPtr[0] = 1u;

	vk::BufferWithMemory				indexBuffer			(vk, device, alloc, vk::makeBufferCreateInfo(sizeof(deUint32) * 4, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible);
	deUint32*							indexDataPtr		= reinterpret_cast<deUint32*>(indexBuffer.getAllocation().getHostPtr());
	indexDataPtr[0] = 0u;
	indexDataPtr[1] = 1u;
	indexDataPtr[2] = 2u;
	indexDataPtr[3] = 3u;

	const vk::VkDeviceSize				bufferSize			= 64;
	de::MovePtr<vk::BufferWithMemory>	buffer				= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, alloc, vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible));

	// Do a dummy run, to ensure memory allocations are done with before performance testing
	{
		vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *dummyPipeline);
		vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::endRendering(vk, *cmdBuffer);
		vk::endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	std::chrono::nanoseconds			time				= std::chrono::nanoseconds(0);
	std::chrono::nanoseconds			refTime				= std::chrono::nanoseconds(0);
	std::chrono::nanoseconds			maxTime				= std::chrono::nanoseconds(0);
	std::chrono::nanoseconds			maxRefTime			= std::chrono::nanoseconds(0);

	for (deUint32 i = 0; i < 100; ++i)
	{
		std::chrono::nanoseconds	currentTime;
		std::chrono::nanoseconds	currentRefTime;

		if (m_type == DRAW_BINARY_BIND)
		{
			vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
			const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
			vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
			currentTime = std::chrono::high_resolution_clock::now() - shaderObjectStart;
			vk::endCommandBuffer(vk, *cmdBuffer);

			vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
			const auto refShaderObjectStart = std::chrono::high_resolution_clock::now();
			vk::bindGraphicsShaders(vk, *cmdBuffer, refShaders[0], refShaders[2], refShaders[3], refShaders[4], refShaders[1], taskSupported, meshSupported);
			currentRefTime = std::chrono::high_resolution_clock::now() - refShaderObjectStart;
			vk::endCommandBuffer(vk, *cmdBuffer);
		}
		else
		{
			vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
			vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
			vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, topology, false, !m_context.getExtendedDynamicStateFeaturesEXT().extendedDynamicState);
			vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
			currentTime = draw(vk, *cmdBuffer, *indexBuffer, *indirectBuffer, *countBuffer);
			vk::endRendering(vk, *cmdBuffer);
			vk::endCommandBuffer(vk, *cmdBuffer);
			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			if (m_type == DRAW_LINKED_SHADERS || m_type == DRAW_BINARY)
			{
				vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
				vk::bindGraphicsShaders(vk, *cmdBuffer, refShaders[0], refShaders[2], refShaders[3], refShaders[4], refShaders[1], taskSupported, meshSupported);
				vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, topology, false, !m_context.getExtendedDynamicStateFeaturesEXT().extendedDynamicState);
				vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
				currentRefTime = draw(vk, *cmdBuffer, *indexBuffer, *indirectBuffer, *countBuffer);
				vk::endRendering(vk, *cmdBuffer);
				vk::endCommandBuffer(vk, *cmdBuffer);
				submitCommandsAndWait(vk, device, queue, *cmdBuffer);
			}
			else
			{
				vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
				vk::VkDeviceSize offset = 0u;
				vk::VkDeviceSize stride = 16u;
				vk.cmdBindVertexBuffers2(*cmdBuffer, 0u, 1u, &**buffer, &offset, &bufferSize, &stride);
				vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, topology, false, !m_context.getExtendedDynamicStateFeaturesEXT().extendedDynamicState);
				vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
				vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
				currentRefTime = draw(vk, *cmdBuffer, *indexBuffer, *indirectBuffer, *countBuffer);
				vk::endRendering(vk, *cmdBuffer);
				vk::endCommandBuffer(vk, *cmdBuffer);
				submitCommandsAndWait(vk, device, queue, *cmdBuffer);
			}
		}

		time += currentTime;
		if (currentTime > maxTime)
			maxTime = currentTime;

		refTime += currentRefTime;
		if (currentRefTime > maxRefTime)
			maxRefTime = currentRefTime;
	}

	for (const auto& shader : refShaders)
		vk.destroyShaderEXT(device, shader, DE_NULL);

	if (m_type == DRAW_STATIC_PIPELINE)
	{
		if (maxTime > maxRefTime * 1.5f)
			return tcu::TestStatus::fail("Maximum shader object rendering iteration was more than 50% slower than maximum static pipeline iteration rendering");
		if (time > refTime * 1.25f)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Shader object rendering was more than 25% slower than static pipeline rendering");
		if (maxTime > maxRefTime * 1.25f)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Maximum shader object iteration rendering was more than 25% slower than maximum static pipeline iteration rendering");
	}
	else if (m_type == DRAW_DYNAMIC_PIPELINE)
	{
		if (maxTime > maxRefTime * 1.2f)
			return tcu::TestStatus::fail("Maximum shader object iteration rendering was more than 20% slower than maximum dynamic pipeline iteration rendering");
		if (time > refTime * 1.1f)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Shader object rendering was more than 10% slower than dynamic pipeline rendering");
		if (maxTime > maxRefTime * 1.1f)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Maximum shader object iteration rendering was more than 10% slower than maximum dynamic pipeline iteration rendering");
	}
	else if (m_type == DRAW_LINKED_SHADERS)
	{
		if (maxTime > maxRefTime * 1.05f)
			return tcu::TestStatus::fail("Maximum unlinked shader object iteration rendering was more than 5% slower than maximum linked shader object iteration rendering");
		if (time * 1.05f < refTime)
			return tcu::TestStatus::fail("Linked shader object rendering was more than 5% slower than unlinked shader object rendering");
		if (maxTime * 1.05f < maxRefTime)
			return tcu::TestStatus::fail("Maximum linked shader object iteration rendering was more than 5% slower than maximum unlinked shader object iteratino rendering");
	}
	else if (m_type == DRAW_BINARY)
	{
		if (maxTime > maxRefTime * 1.05f)
			return tcu::TestStatus::fail("Maximum shader object iteration rendering was more than 5% slower than maximum binary shader object iteration rendering");
		if (time * 1.05f < refTime)
			return tcu::TestStatus::fail("Binary shader object rendering was more than 5% slower than shader object rendering");
		if (maxTime * 1.05f < maxRefTime)
			return tcu::TestStatus::fail("Maximum binary shader object iteration rendering was more than 5% slower than maximum shader object iteration rendering");
	}
	else if (m_type == DRAW_BINARY_BIND)
	{
		if (maxTime > maxRefTime * 1.05f)
			return tcu::TestStatus::fail("Maximum shader object iteration binding was more than 5% slower than maximum binary shader object iteration binding");
		if (time * 1.05f < refTime)
			return tcu::TestStatus::fail("Binary shader object binding was more than 5% slower than shader object binding");
		if (maxTime * 1.05f < maxRefTime)
			return tcu::TestStatus::fail("Maximum binary shader object iteration binding was more than 5% slower than maximum shader object iteration binding");
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectPerformanceCase : public vkt::TestCase
{
public:
	ShaderObjectPerformanceCase(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const DrawType drawType, const TestType& type)
		: vkt::TestCase	(testCtx, name, description)
		, m_drawType	(drawType)
		, m_type		(type)
	{}
	virtual			~ShaderObjectPerformanceCase(void) {}

	void			checkSupport(vkt::Context& context) const override;
	virtual void	initPrograms(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance(Context& context) const override { return new ShaderObjectPerformanceInstance(context, m_drawType, m_type); }

private:
	DrawType	m_drawType;
	TestType	m_type;
};

void ShaderObjectPerformanceCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void ShaderObjectPerformanceCase::initPrograms(vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);

	std::stringstream dummyVert;
	std::stringstream dummyFrag;

	dummyVert
		<< "#version 450\n"
		<< "layout(location = 0) out vec4 rgba;\n"
		<< "void main() {\n"
		<< "    vec2 pos2 = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "	vec3 pos3 = vec3(pos2, 0.0f) * gl_InstanceIndex;\n"
		<< "    gl_Position = vec4(pos3, 1.0f);\n"
		<< "    rgba = vec4(0.0f, pos3.zyx);\n"
		<< "}\n";

	dummyFrag
		<< "#version 450\n"
		<< "layout(location = 0) in vec4 rgba;\n"
		<< "layout(location = 0) out vec4 color;\n"
		<< "void main() {\n"
		<< "	color = rgba * rgba;\n"
		<< "}\n";

	programCollection.glslSources.add("dummyVert") << glu::VertexSource(dummyVert.str());
	programCollection.glslSources.add("dummyFrag") << glu::FragmentSource(dummyFrag.str());
}


class ShaderObjectDispatchPerformanceInstance : public vkt::TestInstance
{
public:
							ShaderObjectDispatchPerformanceInstance		(Context& context, const DispatchType dispatchType)
																		: vkt::TestInstance	(context)
																		, m_dispatchType	(dispatchType)
																		{}
	virtual					~ShaderObjectDispatchPerformanceInstance	(void) {}

	tcu::TestStatus			iterate										(void) override;

private:
	const DispatchType	m_dispatchType;
};

tcu::TestStatus ShaderObjectDispatchPerformanceInstance::iterate (void)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&								alloc						= m_context.getDefaultAllocator();

	const vk::VkDeviceSize				bufferSizeBytes				= sizeof(deUint32) * 16;
	const vk::BufferWithMemory			outputBuffer				(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;

	const auto& binaries			= m_context.getBinaryCollection();

	const auto& comp				= binaries.get("comp");
	const auto  compShaderModule	= createShaderModule(vk, device, comp);

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const auto	pipelineLayout	= makePipelineLayout(vk, device, descriptorSetLayout.get());

	const vk::Unique<vk::VkDescriptorSet>	descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::VkDescriptorBufferInfo		descriptorInfo = vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const auto							compShader		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_COMPUTE_BIT, binaries.get("comp"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
	const vk::VkPipelineCreateFlags		pipelineFlags	= (m_dispatchType == DISPATCH) ? (vk::VkPipelineCreateFlags)0u : (vk::VkPipelineCreateFlags)vk::VK_PIPELINE_CREATE_DISPATCH_BASE_BIT;
	const auto							computePipeline	= vk::makeComputePipeline(vk, device, pipelineLayout.get(), pipelineFlags, nullptr, compShaderModule.get(), (vk::VkPipelineShaderStageCreateFlags)0u);

	const vk::Move<vk::VkCommandPool>	cmdPool			(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	vk::BufferWithMemory				indirectBuffer	(vk, device, alloc, vk::makeBufferCreateInfo(sizeof(vk::VkDispatchIndirectCommand), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	vk::VkDispatchIndirectCommand*		indirectDataPtr = reinterpret_cast<vk::VkDispatchIndirectCommand*>(indirectBuffer.getAllocation().getHostPtr());
	indirectDataPtr->x = 1;
	indirectDataPtr->y = 1;
	indirectDataPtr->z = 1;

	std::chrono::nanoseconds			time	= std::chrono::nanoseconds(0);
	std::chrono::nanoseconds			refTime = std::chrono::nanoseconds(0);

	for (deUint32 i = 0; i < 100; ++i)
	{
		vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);
		vk::bindComputeShader(vk, *cmdBuffer, *compShader);
		if (m_dispatchType == DISPATCH)
		{
			const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
			time += std::chrono::high_resolution_clock::now() - shaderObjectStart;
		}
		else if (m_dispatchType == DISPATCH_BASE)
		{
			const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatchBase(*cmdBuffer, 1, 1, 1, 0, 0, 0);
			time += std::chrono::high_resolution_clock::now() - shaderObjectStart;
		}
		else if (m_dispatchType == DISPATCH_INDIRECT)
		{
			const auto shaderObjectStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatchIndirect(*cmdBuffer, *indirectBuffer, 0u);
			time += std::chrono::high_resolution_clock::now() - shaderObjectStart;
		}
		vk::endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		if (m_dispatchType == DISPATCH)
		{
			const auto pipelineStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
			refTime += std::chrono::high_resolution_clock::now() - pipelineStart;
		}
		else if (m_dispatchType == DISPATCH_BASE)
		{
			const auto pipelineStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatchBase(*cmdBuffer, 1, 1, 1, 0, 0, 0);
			refTime += std::chrono::high_resolution_clock::now() - pipelineStart;
		}
		else if (m_dispatchType == DISPATCH_INDIRECT)
		{
			const auto pipelineStart = std::chrono::high_resolution_clock::now();
			vk.cmdDispatchIndirect(*cmdBuffer, *indirectBuffer, 0u);
			refTime += std::chrono::high_resolution_clock::now() - pipelineStart;
		}
		vk::endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		// Ignore first iteration, there is a penalty on the first call
		if (i == 0)
		{
			time = std::chrono::nanoseconds(0);
			refTime = std::chrono::nanoseconds(0);
		}
	}

	if (time > refTime * 1.05f)
		return tcu::TestStatus::fail("Shader object dispatch was more than 5% slower than compute pipeline dispatch");
	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectDispatchPerformanceCase : public vkt::TestCase
{
public:
					ShaderObjectDispatchPerformanceCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const DispatchType dispatchType)
															: vkt::TestCase		(testCtx, name, description)
															, m_dispatchType	(dispatchType)
															{}
	virtual			~ShaderObjectDispatchPerformanceCase	(void) {}

	void			checkSupport							(vkt::Context& context) const override;
	virtual void	initPrograms							(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance							(Context& context) const override { return new ShaderObjectDispatchPerformanceInstance(context, m_dispatchType); }

private:
	const DispatchType	m_dispatchType;
};

void ShaderObjectDispatchPerformanceCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void ShaderObjectDispatchPerformanceCase::initPrograms (vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);
}


class ShaderObjectBinaryPerformanceInstance : public vkt::TestInstance
{
public:
							ShaderObjectBinaryPerformanceInstance	(Context& context, BinaryType type)
																	: vkt::TestInstance	(context)
																	, m_type			(type)
																	{}
	virtual					~ShaderObjectBinaryPerformanceInstance	(void) {}

	tcu::TestStatus			iterate									(void) override;
private:
	const BinaryType	m_type;
};

tcu::TestStatus ShaderObjectBinaryPerformanceInstance::iterate (void)
{
	const vk::DeviceInterface&	vk						= m_context.getDeviceInterface();
	const vk::VkDevice			device					= m_context.getDevice();
	auto&						alloc					= m_context.getDefaultAllocator();
	const bool					tessellationSupported	= m_context.getDeviceFeatures().tessellationShader;
	const bool					geometrySupported		= m_context.getDeviceFeatures().geometryShader;

	const auto&					binaries				= m_context.getBinaryCollection();

	std::chrono::nanoseconds	time					= std::chrono::nanoseconds(0);
	std::chrono::nanoseconds	refTime					= std::chrono::nanoseconds(0);

	for (deUint32 i = 0; i < 100; ++i)
	{
		const auto					spirvCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported);
		vk::VkShaderEXT				spirvShader;
		const auto spirvStart = std::chrono::high_resolution_clock::now();
		vk.createShadersEXT(device, 1u, &spirvCreateInfo, DE_NULL, &spirvShader);
		const auto spirvEnd = std::chrono::high_resolution_clock::now();
		if (m_type == BINARY_SHADER_CREATE)
			refTime += spirvEnd - spirvStart;

		size_t						dataSize = 0;
		vk.getShaderBinaryDataEXT(device, spirvShader, &dataSize, DE_NULL);
		std::vector<deUint8>		data(dataSize);
		vk.getShaderBinaryDataEXT(device, spirvShader, &dataSize, data.data());

		const vk::VkShaderCreateInfoEXT binaryShaderCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkShaderCreateFlagsEXT		flags;
			vk::VK_SHADER_STAGE_VERTEX_BIT,					// VkShaderStageFlagBits		stage;
			0u,												// VkShaderStageFlags			nextStage;
			vk::VK_SHADER_CODE_TYPE_BINARY_EXT,				// VkShaderCodeTypeEXT			codeType;
			dataSize,										// size_t						codeSize;
			data.data(),									// const void*					pCode;
			"main",											// const char*					pName;
			0u,												// uint32_t						setLayoutCount;
			DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
			0u,												// uint32_t						pushConstantRangeCount;
			DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
			DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
		};

		vk::VkShaderEXT binaryShader;
		const auto binaryStart = std::chrono::high_resolution_clock::now();
		vk.createShadersEXT(device, 1, &binaryShaderCreateInfo, DE_NULL, &binaryShader);
		time += std::chrono::high_resolution_clock::now() - binaryStart;

		const auto					bufferCreateInfo	= vk::makeBufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vk::Move<vk::VkBuffer>		buffer				= vk::createBuffer(vk, device, &bufferCreateInfo);
		const auto					bufferMemReqs		= vk::getBufferMemoryRequirements(vk, device, *buffer);
		const auto					memoryProperties	= vk::getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
		const auto					hostCachedDeviceLocal	= bufferMemReqs.memoryTypeBits & vk::getCompatibleMemoryTypes(memoryProperties, vk::MemoryRequirement::Cached | vk::MemoryRequirement::Local | vk::MemoryRequirement::HostVisible);

		vk::MemoryRequirement		memoryRequirements	= (hostCachedDeviceLocal != 0) ?
														  vk::MemoryRequirement::Cached | vk::MemoryRequirement::Local | vk::MemoryRequirement::HostVisible :
														  vk::MemoryRequirement::Coherent | vk::MemoryRequirement::Local | vk::MemoryRequirement::HostVisible;
		vk::BufferWithMemory		bufferWithMemory	(vk, device, alloc, bufferCreateInfo, memoryRequirements);
		const vk::Allocation&		bufferAlloc			= bufferWithMemory.getAllocation();
		const auto memcpyStart = std::chrono::high_resolution_clock::now();
		memcpy(bufferAlloc.getHostPtr(), data.data(), dataSize);
		flushAlloc(vk, device, bufferAlloc);
		const auto memcpyEnd = std::chrono::high_resolution_clock::now();
		if (m_type == BINARY_MEMCPY)
			refTime += memcpyEnd - memcpyStart;

		vk.destroyShaderEXT(device, spirvShader, DE_NULL);
		vk.destroyShaderEXT(device, binaryShader, DE_NULL);
	}

	if (m_type == BINARY_SHADER_CREATE)
	{
		if (time > refTime * 1.05f)
			return tcu::TestStatus::fail("Binary shader object create time is more than 5% slower than spirv shader object create time");
	}
	else if (m_type == BINARY_MEMCPY)
	{
		if (time > refTime * 1.5f)
			return tcu::TestStatus::fail("Binary shader object create time is more than 50% slower than memcpy of an equal amount of data");
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectBinaryPerformanceCase : public vkt::TestCase
{
public:
					ShaderObjectBinaryPerformanceCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, BinaryType type)
														: vkt::TestCase		(testCtx, name, description)
														, m_type			(type)
														{}
	virtual			~ShaderObjectBinaryPerformanceCase	(void) {}

	void			checkSupport						(vkt::Context& context) const override;
	virtual void	initPrograms						(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance						(Context& context) const override { return new ShaderObjectBinaryPerformanceInstance(context, m_type); }
private:
	const BinaryType	m_type;
};

void ShaderObjectBinaryPerformanceCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void ShaderObjectBinaryPerformanceCase::initPrograms (vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);
}

}


tcu::TestCaseGroup* createShaderObjectPerformanceTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> performanceGroup(new tcu::TestCaseGroup(testCtx, "performance", ""));

	const struct
	{
		DrawType	drawType;
		const char* name;
	} drawTypeTests[] =
	{
		{ DRAW,							"draw"							},
		{ DRAW_INDEXED,					"draw_indexed"					},
		{ DRAW_INDEXED_INDIRECT,		"draw_indexed_indirect"			},
		{ DRAW_INDEXED_INDIRECT_COUNT,	"draw_indexed_indirect_count"	},
		{ DRAW_INDIRECT,				"draw_indirect"					},
		{ DRAW_INDIRECT_COUNT,			"draw_indirect_count"			},
	};

	const struct
	{
		TestType	testTpye;
		const char* name;
	} typeTests[] =
	{
		{ DRAW_STATIC_PIPELINE,		"static_pipeline"	},
		{ DRAW_DYNAMIC_PIPELINE,	"dynamic_pipeline"	},
		{ DRAW_LINKED_SHADERS,		"linked_shaders"	},
		{ DRAW_BINARY,				"binary_shaders"	},
	};

	for (const auto& drawType : drawTypeTests)
	{
		for (const auto& typeTest : typeTests)
		{
			performanceGroup->addChild(new ShaderObjectPerformanceCase(testCtx, std::string(drawType.name) + "_" + std::string(typeTest.name), "", drawType.drawType, typeTest.testTpye));
		}
	}
	performanceGroup->addChild(new ShaderObjectPerformanceCase(testCtx, "binary_bind_shaders", "", DRAW, DRAW_BINARY_BIND));

	performanceGroup->addChild(new ShaderObjectDispatchPerformanceCase(testCtx, "dispatch", "", DISPATCH));
	performanceGroup->addChild(new ShaderObjectDispatchPerformanceCase(testCtx, "dispatch_base", "", DISPATCH_BASE));
	performanceGroup->addChild(new ShaderObjectDispatchPerformanceCase(testCtx, "dispatch_indirect", "", DISPATCH_INDIRECT));

	performanceGroup->addChild(new ShaderObjectBinaryPerformanceCase(testCtx, "binary_shader_create", "", BINARY_SHADER_CREATE));
	performanceGroup->addChild(new ShaderObjectBinaryPerformanceCase(testCtx, "binary_memcpy", "", BINARY_MEMCPY));

	return performanceGroup.release();
}

} // ShaderObject
} // vkt
