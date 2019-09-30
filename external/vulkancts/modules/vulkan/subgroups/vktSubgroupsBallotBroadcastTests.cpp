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

#include "vktSubgroupsBallotBroadcastTests.hpp"
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
	OPTYPE_BROADCAST = 0,
	OPTYPE_BROADCAST_FIRST,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return vkt::subgroups::check(datas, width, 3);
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 3);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_BROADCAST:
			return "subgroupBroadcast";
		case OPTYPE_BROADCAST_FIRST:
			return "subgroupBroadcastFirst";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				extShaderSubGroupBallotTests;
};

std::string getBodySource(CaseDefinition caseDef)
{
	std::ostringstream bdy;

	std::string broadcast;
	std::string broadcastFirst;
	std::string mask;
	int max;
	if (caseDef.extShaderSubGroupBallotTests)
	{
		broadcast		= "readInvocationARB";
		broadcastFirst	= "readFirstInvocationARB";
		mask			= "mask = ballotARB(true);\n";
		max = 64;

		bdy << "  uint64_t mask;\n"
			<< mask
			<< "  uint sgSize = gl_SubGroupSizeARB;\n"
			<< "  uint sgInvocation = gl_SubGroupInvocationARB;\n";
	}
	else
	{
		broadcast		= "subgroupBroadcast";
		broadcastFirst	= "subgroupBroadcastFirst";
		mask			= "mask = subgroupBallot(true);\n";
		max = (int)subgroups::maxSupportedSubgroupSize();

		bdy << "  uvec4 mask = subgroupBallot(true);\n"
			<< "  uint sgSize = gl_SubgroupSize;\n"
			<< "  uint sgInvocation = gl_SubgroupInvocationID;\n";
	}

	if (OPTYPE_BROADCAST == caseDef.opType)
	{
		bdy	<< "  uint tempResult = 0x3;\n";
		for (int i = 0; i < max; i++)
		{
			bdy << "  {\n"
			<< "    const uint id = "<< i << ";\n"
			<< "    " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< broadcast << "(data1[sgInvocation], id);\n"
			<< "    if ((id < sgSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "    {\n"
			<< "      if (op != data1[id])\n"
			<< "      {\n"
			<< "        tempResult = 0;\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint tempResult = 0;\n"
			<< "  uint firstActive = 0;\n"
			<< "  for (uint i = 0; i < sgSize; i++)\n"
			<< "  {\n"
			<< "    if (subgroupBallotBitExtract(mask, i))\n"
			<< "    {\n"
			<< "      firstActive = i;\n"
			<< "      break;\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempResult |= (" << broadcastFirst << "(data1[sgInvocation]) == data1[firstActive]) ? 0x1 : 0;\n"
			<< "  // make the firstActive invocation inactive now\n"
			<< "  if (firstActive != sgInvocation)\n"
			<< "  {\n"
			<< mask
			<< "    for (uint i = 0; i < sgSize; i++)\n"
			<< "    {\n"
			<< "      if (subgroupBallotBitExtract(mask, i))\n"
			<< "      {\n"
			<< "        firstActive = i;\n"
			<< "        break;\n"
			<< "      }\n"
			<< "    }\n"
			<< "    tempResult |= (" << broadcastFirst << "(data1[sgInvocation]) == data1[firstActive]) ? 0x2 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    // the firstActive invocation didn't partake in the second result so set it to true\n"
			<< "    tempResult |= 0x2;\n"
			<< "  }\n";
	}
   return bdy.str();
}

std::string getHelperFunctionARB(CaseDefinition caseDef)
{
	std::ostringstream bdy;

	if (caseDef.extShaderSubGroupBallotTests == DE_FALSE)
		return "";

	bdy << "bool subgroupBallotBitExtract(uint64_t value, uint index)\n";
	bdy << "{\n";
	bdy << "    if (index > 63)\n";
	bdy << "        return false;\n";
	bdy << "    uint64_t mask = 1ul << index;\n";
	bdy << "    if (bool((value & mask)) == true)\n";
	bdy << "        return true;\n";
	bdy << "    return false;\n";
	bdy << "}\n";
   return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	const string extensionHeader =  (caseDef.extShaderSubGroupBallotTests ?	"#extension GL_ARB_shader_ballot: enable\n"
																			"#extension GL_KHR_shader_subgroup_basic: enable\n"
																			"#extension GL_ARB_gpu_shader_int64: enable\n"
																		:	"#extension GL_KHR_shader_subgroup_ballot: enable\n")
									+ subgroups::getAdditionalExtensionForFormat(caseDef.format);

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	std::string bdyStr = getBodySource(caseDef);
	std::string helperStrARB = getHelperFunctionARB(caseDef);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream				vertex;
		vertex << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extensionHeader.c_str()
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform  Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< helperStrARB.c_str()
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
			<< extensionHeader.c_str()
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< helperStrARB.c_str()
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
			<< extensionHeader.c_str()
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< helperStrARB.c_str()
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
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extensionHeader.c_str()
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< helperStrARB.c_str()
			<< "void main (void)\n"
			<< "{\n"
			<< bdyStr
			<< "  out_color  = float(tempResult);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
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
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	std::string bdyStr = getBodySource(caseDef);
	std::string helperStrARB = getHelperFunctionARB(caseDef);

	const string extensionHeader =  (caseDef.extShaderSubGroupBallotTests ?	"#extension GL_ARB_shader_ballot: enable\n"
																			"#extension GL_KHR_shader_subgroup_basic: enable\n"
																			"#extension GL_ARB_gpu_shader_int64: enable\n"
																		:	"#extension GL_KHR_shader_subgroup_ballot: enable\n")
									+ subgroups::getAdditionalExtensionForFormat(caseDef.format);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< extensionHeader.c_str()
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
			<< "};\n"
			<< "\n"
			<< helperStrARB.c_str()
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< bdyStr
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(src.str()) << buildOptions;
	}
	else
	{
		const string vertex =
			"#version 450\n"
			+ extensionHeader +
			"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			"\n"
			+ helperStrARB +
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
			+ extensionHeader +
			"layout(vertices=1) out;\n"
			"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			"\n"
			+ helperStrARB +
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
			"}\n";

		const string tese =
			"#version 450\n"
			+ extensionHeader +
			"layout(isolines) in;\n"
			"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			"\n"
			+ helperStrARB +
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			"#version 450\n"
			+ extensionHeader +
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			"\n"
			+ helperStrARB +
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			+ extensionHeader +
			"layout(location = 0) out uint result;\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer1\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			+ helperStrARB +
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert") << glu::VertexSource(vertex) << buildOptions;
		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc) << buildOptions;
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese) << buildOptions;
		subgroups::addGeometryShadersFromTemplate(geometry, buildOptions, programCollection.glslSources);
		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment)<< buildOptions;
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	if (caseDef.extShaderSubGroupBallotTests && !context.requireDeviceExtension("VK_EXT_shader_subgroup_ballot"))
	{
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_shader_subgroup_ballot extension");
	}

	if (caseDef.extShaderSubGroupBallotTests && !subgroups::isInt64SupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support int64 data types");
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

	subgroups::SSBOData inputData[1];
	inputData[0].format = caseDef.format;
	inputData[0].layout = subgroups::SSBOData::LayoutStd140;
	inputData[0].numElements = caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
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
		subgroups::SSBOData inputData[1];
		inputData[0].format = caseDef.format;
		inputData[0].layout = subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements = caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkCompute);
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

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotBroadcastTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

	de::MovePtr<tcu::TestCaseGroup> graphicGroupARB(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroupARB(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroupARB(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const std::vector<VkFormat> formats = subgroups::getAllFormats();

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];
		// Vector, boolean and double types are not supported by functions defined in VK_EXT_shader_subgroup_ballot.
		const bool formatTypeIsSupportedARB =
		    format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT || format == VK_FORMAT_R32_SFLOAT;

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			const std::string op = de::toLower(getOpTypeName(opTypeIndex));
			const std::string name = op + "_" + subgroups::getFormatNameForGLSL(format);

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(computeGroupARB.get(), name, "", supportedCheck, initPrograms, test, caseDef);

			}

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(graphicGroupARB.get(), name, "", supportedCheck, initPrograms, test, caseDef);

			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(framebufferGroup.get(), name + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(framebufferGroupARB.get(), name + getShaderStageName(caseDef.shaderStage), "",
								supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> groupARB(new tcu::TestCaseGroup(
		testCtx, "ext_shader_subgroup_ballot", "VK_EXT_shader_subgroup_ballot category tests"));

	groupARB->addChild(graphicGroupARB.release());
	groupARB->addChild(computeGroupARB.release());
	groupARB->addChild(framebufferGroupARB.release());

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "ballot_broadcast", "Subgroup ballot broadcast category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
