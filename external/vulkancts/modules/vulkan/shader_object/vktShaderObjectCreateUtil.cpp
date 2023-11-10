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
 * \brief Utilities for vertex buffers.
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectCreateUtil.hpp"
#include "vktTestCase.hpp"

namespace vk
{

std::string getShaderName (vk::VkShaderStageFlagBits stage)
{
	switch (stage)
	{
	case vk::VK_SHADER_STAGE_VERTEX_BIT:					return "vert";
	case vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return "tesc";
	case vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return "tese";
	case vk::VK_SHADER_STAGE_GEOMETRY_BIT:					return "geom";
	case vk::VK_SHADER_STAGE_FRAGMENT_BIT:					return "frag";
	case vk::VK_SHADER_STAGE_COMPUTE_BIT:					return "comp";
	case vk::VK_SHADER_STAGE_MESH_BIT_EXT:					return "mesh";
	case vk::VK_SHADER_STAGE_TASK_BIT_EXT:					return "task";
	default:
		DE_ASSERT(false);
		break;
	}
	return {};
}

vk::VkShaderStageFlags getShaderObjectNextStages (vk::VkShaderStageFlagBits shaderStage, bool tessellationShaderFeature, bool geometryShaderFeature)
{
	if (shaderStage == vk::VK_SHADER_STAGE_VERTEX_BIT)
	{
		vk::VkShaderStageFlags flags = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		if (tessellationShaderFeature)
			flags |= vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		if (geometryShaderFeature)
			flags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		return flags;
	}
	else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		return vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	}
	else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		vk::VkShaderStageFlags flags = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		if (geometryShaderFeature)
			flags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		return flags;
	}
	else if (shaderStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	else if (shaderStage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
	{
		return vk::VK_SHADER_STAGE_MESH_BIT_EXT;
	}
	else if (shaderStage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
	{
		return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	return 0;
}

Move<VkShaderEXT> createShaderFromBinary (const DeviceInterface& vk, VkDevice device, vk::VkShaderStageFlagBits shaderStage, size_t codeSize, const void* pCode, bool tessellationShaderFeature, bool geometryShaderFeature, vk::VkDescriptorSetLayout descriptorSetLayout)
{
	vk::VkShaderCreateInfoEXT		pCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,								// VkStructureType				sType;
		DE_NULL,																	// const void*					pNext;
		0u,																			// VkShaderCreateFlagsEXT		flags;
		shaderStage,																// VkShaderStageFlagBits		stage;
		getShaderObjectNextStages(shaderStage, tessellationShaderFeature, geometryShaderFeature),	// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_BINARY_EXT,											// VkShaderCodeTypeEXT			codeType;
		codeSize,																	// size_t						codeSize;
		pCode,																		// const void*					pCode;
		"main",																		// const char*					pName;
		(descriptorSetLayout != VK_NULL_HANDLE) ? 1u : 0u,							// uint32_t						setLayoutCount;
		(descriptorSetLayout != VK_NULL_HANDLE) ? &descriptorSetLayout : DE_NULL,	// VkDescriptorSetLayout*		pSetLayouts;
		0u,																			// uint32_t						pushConstantRangeCount;
		DE_NULL,																	// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,																	// const VkSpecializationInfo*	pSpecializationInfo;
	};

	return createShader(vk, device, pCreateInfo);
}

Move<VkShaderEXT> createShader (const DeviceInterface& vk, VkDevice device, const vk::VkShaderCreateInfoEXT& shaderCreateInfo)
{
	VkShaderEXT object = VK_NULL_HANDLE;
	VK_CHECK(vk.createShadersEXT(device, 1u, &shaderCreateInfo, DE_NULL, &object));
	return Move<VkShaderEXT>(check<VkShaderEXT>(object), Deleter<VkShaderEXT>(vk, device, DE_NULL));
}

void addBasicShaderObjectShaders (vk::SourceCollections& programCollection)
{
	std::stringstream vert;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;
	std::stringstream comp;

	vert
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n"
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
		<< "	gl_Position.x *= 1.5f;\n"
		<< "}\n";

	geom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
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

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

vk::VkShaderCreateInfoEXT makeShaderCreateInfo (vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& programBinary, bool tessellationShaderFeature, bool geometryShaderFeature, const vk::VkDescriptorSetLayout* descriptorSetLayout)
{
	vk::VkShaderCreateInfoEXT shaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,						// VkStructureType				sType;
		DE_NULL,															// const void*					pNext;
		0u,																	// VkShaderCreateFlagsEXT		flags;
		stage,																// VkShaderStageFlagBits		stage;
		vk::getShaderObjectNextStages(stage, tessellationShaderFeature, geometryShaderFeature),	// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,									// VkShaderCodeTypeEXT			codeType;
		programBinary.getSize(),											// size_t						codeSize;
		programBinary.getBinary(),											// const void*					pCode;
		"main",																// const char*					pName;
		(descriptorSetLayout != DE_NULL) ? 1u : 0u,							// uint32_t						setLayoutCount;
		(descriptorSetLayout != DE_NULL) ? descriptorSetLayout : DE_NULL,	// VkDescriptorSetLayout*		pSetLayouts;
		0u,																	// uint32_t						pushConstantRangeCount;
		DE_NULL,															// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,															// const VkSpecializationInfo*	pSpecializationInfo;
	};

	return shaderCreateInfo;
}

bool extensionEnabled(const std::vector<std::string>& deviceExtensions, const std::string& ext)
{
	return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) != deviceExtensions.end();
}

void setDefaultShaderObjectDynamicStates (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, const std::vector<std::string>& deviceExtensions, vk::VkPrimitiveTopology topology, bool meshShader, bool setViewport)
{
	vk::VkViewport viewport = { 0, 0, 32, 32, 0.0f, 1.0f, };
	if (setViewport)
		vk.cmdSetViewport(cmdBuffer, 0u, 1u, &viewport);
	vk.cmdSetViewportWithCount(cmdBuffer, 1u, &viewport);
	vk::VkRect2D scissor = { { 0, 0, }, { 32, 32, }, };
	if (setViewport)
		vk.cmdSetScissor(cmdBuffer, 0u, 1u, &scissor);
	vk.cmdSetScissorWithCount(cmdBuffer, 1u, &scissor);
	vk.cmdSetLineWidth(cmdBuffer, 1.0f);
	vk.cmdSetDepthBias(cmdBuffer, 1.0f, 1.0f, 1.0f);
	float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	vk.cmdSetBlendConstants(cmdBuffer, blendConstants);
	vk.cmdSetDepthBounds(cmdBuffer, 0.0f, 1.0f);
	vk.cmdSetStencilCompareMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	vk.cmdSetStencilWriteMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	vk.cmdSetStencilReference(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	vk.cmdBindVertexBuffers2(cmdBuffer, 0, 0, DE_NULL, DE_NULL, DE_NULL, DE_NULL);
	vk.cmdSetCullMode(cmdBuffer, vk::VK_CULL_MODE_NONE);
	vk.cmdSetDepthBoundsTestEnable(cmdBuffer, VK_FALSE);
	vk.cmdSetDepthCompareOp(cmdBuffer, vk::VK_COMPARE_OP_NEVER);
	vk.cmdSetDepthTestEnable(cmdBuffer, VK_FALSE);
	vk.cmdSetDepthWriteEnable(cmdBuffer, VK_FALSE);
	vk.cmdSetFrontFace(cmdBuffer, vk::VK_FRONT_FACE_CLOCKWISE);
	if (!meshShader)
		vk.cmdSetPrimitiveTopology(cmdBuffer, topology);
	vk.cmdSetStencilOp(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_NEVER);
	vk.cmdSetStencilTestEnable(cmdBuffer, VK_FALSE);
	vk.cmdSetDepthBiasEnable(cmdBuffer, VK_FALSE);
	if (!meshShader)
		vk.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);
	vk.cmdSetRasterizerDiscardEnable(cmdBuffer, VK_FALSE);
	if (!meshShader && (extensionEnabled(deviceExtensions, "VK_EXT_shader_object") || extensionEnabled(deviceExtensions, "VK_EXT_vertex_input_dynamic_state")))
		vk.cmdSetVertexInputEXT(cmdBuffer, 0u, DE_NULL, 0u, DE_NULL);
	vk.cmdSetLogicOpEXT(cmdBuffer, vk::VK_LOGIC_OP_AND);
	if (!meshShader)
		vk.cmdSetPatchControlPointsEXT(cmdBuffer, 4u);
	vk.cmdSetTessellationDomainOriginEXT(cmdBuffer, vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
	vk.cmdSetDepthClampEnableEXT(cmdBuffer, VK_FALSE);
	vk.cmdSetPolygonModeEXT(cmdBuffer, vk::VK_POLYGON_MODE_FILL);
	vk.cmdSetRasterizationSamplesEXT(cmdBuffer, vk::VK_SAMPLE_COUNT_1_BIT);
	vk::VkSampleMask sampleMask = 0xFFFFFFFF;
	vk.cmdSetSampleMaskEXT(cmdBuffer, vk::VK_SAMPLE_COUNT_1_BIT, &sampleMask);
	vk.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, VK_FALSE);
	vk.cmdSetAlphaToOneEnableEXT(cmdBuffer, VK_FALSE);
	vk.cmdSetLogicOpEnableEXT(cmdBuffer, VK_FALSE);
	vk::VkBool32 colorBlendEnable = VK_FALSE;
	vk.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, 1u, &colorBlendEnable);
	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		alphaBlendOp;
	};
	vk.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, 1u, &colorBlendEquation);
	vk::VkColorComponentFlags		colorWriteMask = vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT |
		vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT;
	vk.cmdSetColorWriteMaskEXT(cmdBuffer, 0u, 1u, &colorWriteMask);
	vk::VkExtent2D fragmentSize = { 1u, 1u };
	vk::VkFragmentShadingRateCombinerOpKHR combinerOps[2] = { VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR };
	if (extensionEnabled(deviceExtensions, "VK_KHR_fragment_shading_rate"))
		vk.cmdSetFragmentShadingRateKHR(cmdBuffer, &fragmentSize, combinerOps);
	if (extensionEnabled(deviceExtensions, "VK_EXT_transform_feedback"))
		vk.cmdSetRasterizationStreamEXT(cmdBuffer, 0);
	if (extensionEnabled(deviceExtensions, "VK_EXT_conservative_rasterization"))
		vk.cmdSetConservativeRasterizationModeEXT(cmdBuffer, vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_conservative_rasterization"))
		vk.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, 0.0f);
	if (extensionEnabled(deviceExtensions, "VK_EXT_depth_clip_enable"))
		vk.cmdSetDepthClipEnableEXT(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_EXT_sample_locations"))
		vk.cmdSetSampleLocationsEnableEXT(cmdBuffer, VK_FALSE);
	VkSampleLocationEXT sampleLocation = { 0.5f, 0.5f };
	const vk::VkSampleLocationsInfoEXT sampleLocations =
	{
		vk::VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,	// VkStructureType               sType;
		DE_NULL,											// const void*                   pNext;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits         sampleLocationsPerPixel;
		{ 1u, 1u },											// VkExtent2D                    sampleLocationGridSize;
		1,													// uint32_t                      sampleLocationsCount;
		&sampleLocation,									// const VkSampleLocationEXT*    pSampleLocations;
	};
	if (extensionEnabled(deviceExtensions, "VK_EXT_sample_locations"))
		vk.cmdSetSampleLocationsEXT(cmdBuffer, &sampleLocations);
	vk::VkColorBlendAdvancedEXT colorBlendAdvanced;
	colorBlendAdvanced.advancedBlendOp = vk::VK_BLEND_OP_SRC_EXT;
	colorBlendAdvanced.srcPremultiplied = VK_FALSE;
	colorBlendAdvanced.dstPremultiplied = VK_FALSE;
	colorBlendAdvanced.blendOverlap = vk::VK_BLEND_OVERLAP_UNCORRELATED_EXT;
	colorBlendAdvanced.clampResults = VK_FALSE;
	if (extensionEnabled(deviceExtensions, "VK_EXT_blend_operation_advanced"))
		vk.cmdSetColorBlendAdvancedEXT(cmdBuffer, 0, 1, &colorBlendAdvanced);
	if (extensionEnabled(deviceExtensions, "VK_EXT_provoking_vertex"))
		vk.cmdSetProvokingVertexModeEXT(cmdBuffer, vk::VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization"))
		vk.cmdSetLineRasterizationModeEXT(cmdBuffer, vk::VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization"))
		vk.cmdSetLineStippleEnableEXT(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_EXT_line_rasterization"))
		vk.cmdSetLineStippleEXT(cmdBuffer, 1u, 0x0F0F);
	if (extensionEnabled(deviceExtensions, "VK_EXT_depth_clip_control"))
		vk.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, VK_FALSE);
	VkBool32 colorWriteEnable = VK_TRUE;
	if (extensionEnabled(deviceExtensions, "VK_EXT_color_write_enable"))
		vk.cmdSetColorWriteEnableEXT(cmdBuffer, 1, &colorWriteEnable);
	if (extensionEnabled(deviceExtensions, "VK_NV_clip_space_w_scaling"))
		vk.cmdSetViewportWScalingEnableNV(cmdBuffer, VK_FALSE);
	vk::VkViewportWScalingNV viewportWScaling = { 1.0f, 1.0f };
	if (extensionEnabled(deviceExtensions, "VK_NV_clip_space_w_scaling"))
		vk.cmdSetViewportWScalingNV(cmdBuffer, 0, 1, &viewportWScaling);
	vk::VkViewportSwizzleNV viewportSwizzle;
	viewportSwizzle.x = VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV;
	viewportSwizzle.y = VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV;
	viewportSwizzle.z = VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV;
	viewportSwizzle.w = VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV;
	if (extensionEnabled(deviceExtensions, "VK_NV_viewport_swizzle"))
		vk.cmdSetViewportSwizzleNV(cmdBuffer, 0, 1, &viewportSwizzle);
	if (extensionEnabled(deviceExtensions, "VK_NV_fragment_coverage_to_color"))
		vk.cmdSetCoverageToColorEnableNV(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_NV_fragment_coverage_to_color"))
		vk.cmdSetCoverageToColorLocationNV(cmdBuffer, 0);
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples"))
		vk.cmdSetCoverageModulationModeNV(cmdBuffer, vk::VK_COVERAGE_MODULATION_MODE_NONE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples"))
		vk.cmdSetCoverageModulationTableEnableNV(cmdBuffer, VK_FALSE);
	float coverageModulationTable = 1.0f;
	if (extensionEnabled(deviceExtensions, "VK_NV_framebuffer_mixed_samples"))
		vk.cmdSetCoverageModulationTableNV(cmdBuffer, 1, &coverageModulationTable);
	if (extensionEnabled(deviceExtensions, "VK_NV_shading_rate_image"))
		vk.cmdSetShadingRateImageEnableNV(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_NV_coverage_reduction_mode"))
		vk.cmdSetCoverageReductionModeNV(cmdBuffer, vk::VK_COVERAGE_REDUCTION_MODE_MERGE_NV);
	if (extensionEnabled(deviceExtensions, "VK_NV_representative_fragment_test"))
		vk.cmdSetRepresentativeFragmentTestEnableNV(cmdBuffer, VK_FALSE);
	vk::VkBool32 scissorEnable = VK_FALSE;
	if (extensionEnabled(deviceExtensions, "VK_NV_scissor_exclusive"))
		vk.cmdSetExclusiveScissorEnableNV(cmdBuffer, 0u, 1u, &scissorEnable);
	if (extensionEnabled(deviceExtensions, "VK_NV_scissor_exclusive"))
		vk.cmdSetExclusiveScissorNV(cmdBuffer, 0u, 1u, &scissor);
	if (extensionEnabled(deviceExtensions, "VK_NV_fragment_shading_rate_enums"))
		vk.cmdSetFragmentShadingRateEnumNV(cmdBuffer, vk::VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV, combinerOps);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		vk.cmdSetDiscardRectangleEnableEXT(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		vk.cmdSetDiscardRectangleEXT(cmdBuffer, 0u, 1u, &scissor);
	if (extensionEnabled(deviceExtensions, "VK_EXT_discard_rectangles"))
		vk.cmdSetDiscardRectangleModeEXT(cmdBuffer, vk::VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT);
	if (extensionEnabled(deviceExtensions, "VK_NV_shading_rate_image"))
		vk.cmdSetShadingRateImageEnableNV(cmdBuffer, VK_FALSE);
	if (extensionEnabled(deviceExtensions, "VK_EXT_attachment_feedback_loop_dynamic_state"))
		vk.cmdSetAttachmentFeedbackLoopEnableEXT(cmdBuffer, 0u);
}

void bindGraphicsShaders (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkShaderEXT vertShader, vk::VkShaderEXT tescShader, vk::VkShaderEXT teseShader, vk::VkShaderEXT geomShader, vk::VkShaderEXT fragShader, bool taskShaderSupported, bool meshShaderSupported)
{
	vk::VkShaderStageFlagBits stages[] = {
			vk::VK_SHADER_STAGE_VERTEX_BIT,
			vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			vk::VK_SHADER_STAGE_GEOMETRY_BIT,
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	vk::VkShaderEXT shaders[] = {
		vertShader,
		tescShader,
		teseShader,
		geomShader,
		fragShader,
	};
	vk.cmdBindShadersEXT(cmdBuffer, 5u, stages, shaders);
	if (taskShaderSupported) {
		vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
		vk::VkShaderEXT shader = VK_NULL_HANDLE;
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &stage, &shader);
	}
	if (meshShaderSupported) {
		vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_MESH_BIT_EXT;
		vk::VkShaderEXT shader = VK_NULL_HANDLE;
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &stage, &shader);
	}
}

void bindComputeShader (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkShaderEXT compShader)
{
	vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
	vk.cmdBindShadersEXT(cmdBuffer, 1, &stage, &compShader);
}

void bindNullTaskMeshShaders (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures)
{
	vk::VkShaderEXT shader = VK_NULL_HANDLE;
	vk::VkShaderStageFlagBits taskStage = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
	vk::VkShaderStageFlagBits meshStage = vk::VK_SHADER_STAGE_MESH_BIT_EXT;
	if (meshShaderFeatures.taskShader) {
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &taskStage, &shader);
	}
	if (meshShaderFeatures.meshShader) {
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &meshStage, &shader);
	}
}

void bindNullRasterizationShaders (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkPhysicalDeviceFeatures features)
{
	vk::VkShaderEXT shader = VK_NULL_HANDLE;
	vk::VkShaderStageFlagBits vertStage = vk::VK_SHADER_STAGE_VERTEX_BIT;
	vk::VkShaderStageFlagBits tescStage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	vk::VkShaderStageFlagBits teseStage = vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	vk::VkShaderStageFlagBits geomStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
	vk.cmdBindShadersEXT(cmdBuffer, 1u, &vertStage, &shader);
	if (features.tessellationShader) {
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &tescStage, &shader);
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &teseStage, &shader);
	}
	if (features.geometryShader) {
		vk.cmdBindShadersEXT(cmdBuffer, 1u, &geomStage, &shader);
	}
}

} // vk
