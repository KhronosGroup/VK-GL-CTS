/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotMasksTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{

enum MaskType
{
	MASKTYPE_EQ = 0,
	MASKTYPE_GE,
	MASKTYPE_GT,
	MASKTYPE_LE,
	MASKTYPE_LT,
	MASKTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return vkt::subgroups::check(datas, width, 0xf);
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 0xf);
}

std::string getMaskTypeName(int maskType)
{
	switch (maskType)
	{
	default:
		DE_FATAL("Unsupported mask type");
		return "";
	case MASKTYPE_EQ:
		return "gl_SubGroupEqMaskARB";
	case MASKTYPE_GE:
		return "gl_SubGroupGeMaskARB";
	case MASKTYPE_GT:
		return "gl_SubGroupGtMaskARB";
	case MASKTYPE_LE:
		return "gl_SubGroupLeMaskARB";
	case MASKTYPE_LT:
		return "gl_SubGroupLtMaskARB";
	}
}


struct CaseDefinition
{
	int					maskType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

std::string getBodySource(CaseDefinition caseDef)
{
	std::ostringstream bdy;

	bdy << "uint64_t value = " << getMaskTypeName(caseDef.maskType) << ";\n";
	bdy << "bool temp = true;\n";

	switch(caseDef.maskType)
	{
	case MASKTYPE_EQ:
		bdy << "uint64_t mask = uint64_t(1) << gl_SubGroupInvocationARB;\n";
		bdy << "temp = (value & mask) != 0;\n";
		break;
	case MASKTYPE_GE:
		bdy << "for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n";
		bdy << "  uint64_t mask = uint64_t(1) << i;\n";
		bdy << "  if (i >= gl_SubGroupInvocationARB && (value & mask) == 0)\n";
		bdy << "     temp = false;\n";
		bdy << "  if (i < gl_SubGroupInvocationARB && (value & mask) != 0)\n";
		bdy << "     temp = false;\n";
		bdy << "};\n";
		break;
	case MASKTYPE_GT:
		bdy << "for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n";
		bdy << "  uint64_t mask = uint64_t(1) << i;\n";
		bdy << "  if (i > gl_SubGroupInvocationARB && (value & mask) == 0)\n";
		bdy << "     temp = false;\n";
		bdy << "  if (i <= gl_SubGroupInvocationARB && (value & mask) != 0)\n";
		bdy << "     temp = false;\n";
		bdy << "};\n";
		break;
	case MASKTYPE_LE:
		bdy << "for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n";
		bdy << "  uint64_t mask = uint64_t(1) << i;\n";
		bdy << "  if (i <= gl_SubGroupInvocationARB && (value & mask) == 0)\n";
		bdy << "     temp = false;\n";
		bdy << "  if (i > gl_SubGroupInvocationARB && (value & mask) != 0)\n";
		bdy << "     temp = false;\n";
		bdy << "};\n";
		break;
	case MASKTYPE_LT:
		bdy << "for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n";
		bdy << "  uint64_t mask = uint64_t(1) << i;\n";
		bdy << "  if (i < gl_SubGroupInvocationARB && (value & mask) == 0)\n";
		bdy << "     temp = false;\n";
		bdy << "  if (i >= gl_SubGroupInvocationARB && (value & mask) != 0)\n";
		bdy << "     temp = false;\n";
		bdy << "};\n";
		break;
	}

	bdy << "uint tempResult = temp ? 0xf : 0x2;\n";
	return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	std::string bdyStr = getBodySource(caseDef);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream				vertex;
		vertex << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";
		programCollection.glslSources.add("vert")
			<< glu::VertexSource(vertex.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.glslSources.add("geometry")
			<< glu::GeometrySource(geometry.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		std::ostringstream controlSource;

		controlSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<< "  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< bdyStr
			<< "  out_color[gl_InvocationID ] = float(tempResult);\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "")
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color  = float(tempResult);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese")
			<< glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string bdyStr = getBodySource(caseDef);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_ARB_shader_ballot: enable\n"
			<< "#extension GL_ARB_gpu_shader_int64: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< bdyStr
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string vertex =
			"#version 450\n"
			"#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_VertexIndex] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			"#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			"layout(vertices=1) out;\n"
			"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveID] = tempResult;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			+ (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "") +
			"}\n";

		const string tese =
			"#version 450\n"
			"#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			"layout(isolines) in;\n"
			"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
			"}\n";

		const string geometry =
			"#version 450\n"
			"#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			"#extension GL_ARB_shader_ballot: enable\n"
			"#extension GL_ARB_gpu_shader_int64: enable\n"
			"layout(location = 0) out uint result;\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
												  programCollection.glslSources);
		programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!context.requireDeviceFunctionality("VK_EXT_shader_subgroup_ballot"))
	{
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_shader_subgroup_ballot extension");
	}

	if (!subgroups::isInt64SupportedForDevice(context))
		TCU_THROW(NotSupportedError, "Int64 is not supported");

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
			context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if ((VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) & caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
				subgroups::getShaderStageName(caseDef.shaderStage) +
				" is required to support subgroup operations!");
		}
		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkCompute);
	}
	else
	{
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);

		if ( VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, stages);
	}
	return tcu::TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotMasksTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "VK_EXT_shader_subgroup_ballot masks category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "VK_EXT_shader_subgroup_ballot masks category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "VK_EXT_shader_subgroup_ballot masks category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};
	for (int maskTypeIndex = 0; maskTypeIndex < MASKTYPE_LAST; ++maskTypeIndex)
	{
		const string mask = de::toLower(getMaskTypeName(maskTypeIndex));

		{
			const CaseDefinition caseDef = {maskTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool)};
			addFunctionCaseWithPrograms(computeGroup.get(), mask, "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {maskTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, de::SharedPtr<bool>(new bool)};
			addFunctionCaseWithPrograms(graphicGroup.get(), mask, "", supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {maskTypeIndex, stages[stageIndex], de::SharedPtr<bool>(new bool)};
			addFunctionCaseWithPrograms(framebufferGroup.get(), mask + "_" + getShaderStageName(caseDef.shaderStage), "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<tcu::TestCaseGroup> groupARB(new tcu::TestCaseGroup(
		testCtx, "ext_shader_subgroup_ballot", "VK_EXT_shader_subgroup_ballot masks category tests"));

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "ballot_mask", "VK_EXT_shader_subgroup_ballot mask category tests"));

	groupARB->addChild(graphicGroup.release());
	groupARB->addChild(computeGroup.release());
	groupARB->addChild(framebufferGroup.release());
	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
