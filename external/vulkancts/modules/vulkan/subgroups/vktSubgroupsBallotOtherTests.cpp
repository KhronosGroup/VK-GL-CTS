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
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotOtherTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
enum OpType
{
	OPTYPE_INVERSE_BALLOT = 0,
	OPTYPE_BALLOT_BIT_EXTRACT,
	OPTYPE_BALLOT_BIT_COUNT,
	OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_FIND_LSB,
	OPTYPE_BALLOT_FIND_MSB,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::check(datas, width, 0xf);
}

static bool checkCompute(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 0xf);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_INVERSE_BALLOT:
			return "subgroupInverseBallot";
		case OPTYPE_BALLOT_BIT_EXTRACT:
			return "subgroupBallotBitExtract";
		case OPTYPE_BALLOT_BIT_COUNT:
			return "subgroupBallotBitCount";
		case OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT:
			return "subgroupBallotInclusiveBitCount";
		case OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT:
			return "subgroupBallotExclusiveBitCount";
		case OPTYPE_BALLOT_FIND_LSB:
			return "subgroupBallotFindLSB";
		case OPTYPE_BALLOT_FIND_MSB:
			return "subgroupBallotFindMSB";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

std::string getBodySource(CaseDefinition caseDef)
{
	std::ostringstream bdy;

	bdy << "  uvec4 allOnes = uvec4(0xFFFFFFFF);\n"
		<< "  uvec4 allZeros = uvec4(0);\n"
		<< "  uint tempResult = 0;\n"
		<< "#define MAKE_HIGH_BALLOT_RESULT(i) uvec4("
		<< "i >= 32 ? 0 : (0xFFFFFFFF << i), "
		<< "i >= 64 ? 0 : (0xFFFFFFFF << ((i < 32) ? 0 : (i - 32))), "
		<< "i >= 96 ? 0 : (0xFFFFFFFF << ((i < 64) ? 0 : (i - 64))), "
		<< "i >= 128 ? 0 : (0xFFFFFFFF << ((i < 96) ? 0 : (i - 96))))\n"
		<< "#define MAKE_SINGLE_BIT_BALLOT_RESULT(i) uvec4("
		<< "i >= 32 ? 0 : 0x1 << i, "
		<< "i < 32 || i >= 64 ? 0 : 0x1 << (i - 32), "
		<< "i < 64 || i >= 96 ? 0 : 0x1 << (i - 64), "
		<< "i < 96 || i >= 128 ? 0 : 0x1 << (i - 96))\n";

	switch (caseDef.opType)
	{
		default:
			DE_FATAL("Unknown op type!");
			break;
		case OPTYPE_INVERSE_BALLOT:
			bdy << "  tempResult |= subgroupInverseBallot(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= subgroupInverseBallot(allZeros) ? 0 : 0x2;\n"
				<< "  tempResult |= subgroupInverseBallot(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n";
			break;
		case OPTYPE_BALLOT_BIT_EXTRACT:
			bdy << "  tempResult |= subgroupBallotBitExtract(allOnes, gl_SubgroupInvocationID) ? 0x1 : 0;\n"
				<< "  tempResult |= subgroupBallotBitExtract(allZeros, gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				<< "  tempResult |= subgroupBallotBitExtract(subgroupBallot(true), gl_SubgroupInvocationID) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (!subgroupBallotBitExtract(allOnes, gl_SubgroupInvocationID))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_BIT_COUNT:
			bdy << "  /* To ensure a 32-bit computation, use a variable with default highp precision. */\n"
				<< "  uint SubgroupSize = gl_SubgroupSize;\n"
				<< "  tempResult |= SubgroupSize == subgroupBallotBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(MAKE_HIGH_BALLOT_RESULT(SubgroupSize)) ? 0x8 : 0;\n";
			break;
		case OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT:
			bdy << "  uint inclusiveOffset = gl_SubgroupInvocationID + 1;\n"
				<< "  tempResult |= inclusiveOffset == subgroupBallotInclusiveBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotInclusiveBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotInclusiveBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < 128; i++)\n"
				<< "  {\n"
				<< "    uint ref = inclusiveOffset - min(inclusiveOffset, i);\n"
				<< "    uvec4 b = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    uint inclusiveBitCount = subgroupBallotInclusiveBitCount(b);\n"
				<< "    if (inclusiveBitCount != ref)\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT:
			bdy << "  uint exclusiveOffset = gl_SubgroupInvocationID;\n"
				<< "  tempResult |= exclusiveOffset == subgroupBallotExclusiveBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotExclusiveBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0x4;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < 128; i++)\n"
				<< "  {\n"
				<< "    uint ref = exclusiveOffset - min(exclusiveOffset, i);\n"
				<< "    uvec4 b = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    uint exclusiveBitCount = subgroupBallotExclusiveBitCount(b);\n"
				<< "    if (exclusiveBitCount != ref)\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_FIND_LSB:
			bdy << "  tempResult |= 0 == subgroupBallotFindLSB(allOnes) ? 0x1 : 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    tempResult |= 0x2;\n"
				<< "  }\n"
				<< "  else\n"
				<< "  {\n"
				<< "    tempResult |= 0 < subgroupBallotFindLSB(subgroupBallot(true)) ? 0x2 : 0;\n"
				<< "  }\n"
				<< "  tempResult |= gl_SubgroupSize > subgroupBallotFindLSB(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (i != subgroupBallotFindLSB(MAKE_HIGH_BALLOT_RESULT(i)))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_FIND_MSB:
			bdy << "  tempResult |= (gl_SubgroupSize - 1) == subgroupBallotFindMSB(allOnes) ? 0x1 : 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    tempResult |= 0x2;\n"
				<< "  }\n"
				<< "  else\n"
				<< "  {\n"
				<< "    tempResult |= 0 < subgroupBallotFindMSB(subgroupBallot(true)) ? 0x2 : 0;\n"
				<< "  }\n"
				<< "  tempResult |= gl_SubgroupSize > subgroupBallotFindMSB(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (i != subgroupBallotFindMSB(MAKE_SINGLE_BIT_BALLOT_RESULT(i)))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
	}
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out uint result;\n"
			"precision highp int;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (caseDef.requiredSubgroupSize)
	{
		if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
			TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroup_size_control extension");
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

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
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
	else if ((VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) & caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
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
		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkCompute);

		tcu::TestLog& log	= context.getTestContext().getLog();
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;
		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		log << tcu::TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0u, DE_NULL, checkCompute,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
				return result;
			}
		}

		return tcu::TestStatus::pass("OK");
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

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
	return tcu::TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotOtherTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot other category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot other category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot other category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const string op = de::toLower(getOpTypeName(opTypeIndex));
		{
			CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(computeGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
			caseDef.requiredSubgroupSize = DE_TRUE;
			addFunctionCaseWithPrograms(computeGroup.get(), op + "_requiredsubgroupSize", "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(graphicGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], de::SharedPtr<bool>(new bool), DE_FALSE};
			addFunctionCaseWithPrograms(framebufferGroup.get(), op + "_" + getShaderStageName(caseDef.shaderStage), "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "ballot_other", "Subgroup ballot other category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
