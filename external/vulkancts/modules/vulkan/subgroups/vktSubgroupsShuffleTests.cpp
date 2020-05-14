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

#include "vktSubgroupsShuffleTests.hpp"
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
	OPTYPE_SHUFFLE = 0,
	OPTYPE_SHUFFLE_XOR,
	OPTYPE_SHUFFLE_UP,
	OPTYPE_SHUFFLE_DOWN,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return vkt::subgroups::check(datas, width, 1);
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_SHUFFLE:
			return "subgroupShuffle";
		case OPTYPE_SHUFFLE_XOR:
			return "subgroupShuffleXor";
		case OPTYPE_SHUFFLE_UP:
			return "subgroupShuffleUp";
		case OPTYPE_SHUFFLE_DOWN:
			return "subgroupShuffleDown";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

const std::string to_string(int x) {
	std::ostringstream oss;
	oss << x;
	return oss.str();
}

const std::string DeclSource(CaseDefinition caseDef, int baseBinding)
{
	return
		"layout(set = 0, binding = " + to_string(baseBinding) + ", std430) readonly buffer Buffer2\n"
		"{\n"
		"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
		"};\n"
		"layout(set = 0, binding = " + to_string(baseBinding + 1) + ", std430) readonly buffer Buffer3\n"
		"{\n"
		"  uint data2[];\n"
		"};\n";
}

const std::string TestSource(CaseDefinition caseDef)
{
	std::string						idTable[OPTYPE_LAST];
	idTable[OPTYPE_SHUFFLE]			= "id_in";
	idTable[OPTYPE_SHUFFLE_XOR]		= "gl_SubgroupInvocationID ^ id_in";
	idTable[OPTYPE_SHUFFLE_UP]		= "gl_SubgroupInvocationID - id_in";
	idTable[OPTYPE_SHUFFLE_DOWN]	= "gl_SubgroupInvocationID + id_in";

	const std::string testSource =
		"  uint temp_res;\n"
		"  uvec4 mask = subgroupBallot(true);\n"
		"  uint id_in = data2[gl_SubgroupInvocationID] & (gl_SubgroupSize - 1);\n"
		"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " op = "
		+ getOpTypeName(caseDef.opType) + "(data1[gl_SubgroupInvocationID], id_in);\n"
		"  uint id = " + idTable[caseDef.opType] + ";\n"
		"  if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
		"  {\n"
		"    temp_res = (op == data1[id]) ? 1 : 0;\n"
		"  }\n"
		"  else\n"
		"  {\n"
		"    temp_res = 1; // Invocation we read from was inactive, so we can't verify results!\n"
		"  }\n";

	return testSource;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	const std::string extSource =
	(OPTYPE_SHUFFLE == caseDef.opType || OPTYPE_SHUFFLE_XOR == caseDef.opType) ?
		"#extension GL_KHR_shader_subgroup_shuffle: enable\n" :
		"#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";

	const std::string testSource = TestSource(caseDef);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n"
			<< extSource
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1) uniform Buffer2\n"
			<< "{\n"
			<< "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< testSource
			<< "  result = temp_res;\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";
		programCollection.glslSources.add("vert")
			<< glu::VertexSource(vertexSrc.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extSource
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1) uniform Buffer2\n"
			<< "{\n"
			<< "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< testSource
			<< "  out_color = temp_res;\n"
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
			<< extSource
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1) uniform Buffer2\n"
			<< "{\n"
			<< "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< testSource
			<< "  out_color[gl_InvocationID] = temp_res;\n"
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
			<< extSource
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1) uniform Buffer2\n"
			<< "{\n"
			<< "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< testSource
			<< "  out_color = temp_res;\n"
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
	const std::string vSource =
		"#version 450\n"
		"#extension GL_KHR_shader_subgroup_ballot: enable\n";
	const std::string eSource =
	(OPTYPE_SHUFFLE == caseDef.opType || OPTYPE_SHUFFLE_XOR == caseDef.opType) ?
		"#extension GL_KHR_shader_subgroup_shuffle: enable\n" :
		"#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
	const std::string extSource = vSource + eSource + subgroups::getAdditionalExtensionForFormat(caseDef.format);

	const std::string testSource = TestSource(caseDef);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

	src << extSource
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< DeclSource(caseDef, 1)
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< testSource
			<< "  result[offset] = temp_res;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const std::string declSource = DeclSource(caseDef, 4);

		{
			const string vertex =
				extSource +
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				+ declSource +
				"\n"
				"void main (void)\n"
				"{\n"
				+ testSource +
				"  result[gl_VertexIndex] = temp_res;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";

			programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tesc =
				extSource +
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430)  buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				+ declSource +
				"\n"
				"void main (void)\n"
				"{\n"
				+ testSource +
				"  result[gl_PrimitiveID] = temp_res;\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tese =
				extSource +
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				+ declSource +
				"\n"
				"void main (void)\n"
				"{\n"
				+ testSource +
				"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = temp_res;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				extSource +
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				+ declSource +
				"\n"
				"void main (void)\n"
				"{\n"
				+ testSource +
				"  result[gl_PrimitiveIDIn] = temp_res;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}
		{
			const string fragment =
				extSource +
				"layout(location = 0) out uint result;\n"
				+ declSource +
				"void main (void)\n"
				"{\n"
				+ testSource +
				"  result = temp_res;\n"
				"}\n";

			programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	switch (caseDef.opType)
	{
		case OPTYPE_SHUFFLE:
		case OPTYPE_SHUFFLE_XOR:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
			}
			break;
		default:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
			}
			break;
	}

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(
					caseDef.shaderStage))
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

	subgroups::SSBOData inputData[2];
	inputData[0].format = caseDef.format;
	inputData[0].layout = subgroups::SSBOData::LayoutStd140;
	inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

	inputData[1].format = VK_FORMAT_R32_UINT;
	inputData[1].layout = subgroups::SSBOData::LayoutStd140;
	inputData[1].numElements = inputData[0].numElements;
	inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
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
		subgroups::SSBOData inputData[2];
		inputData[0].format = caseDef.format;
		inputData[0].layout = subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		inputData[1].format = VK_FORMAT_R32_UINT;
		inputData[1].layout = subgroups::SSBOData::LayoutStd430;
		inputData[1].numElements = inputData[0].numElements;
		inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 2, checkCompute);
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

		if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData[2];
		inputData[0].format			= caseDef.format;
		inputData[0].layout			= subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements	= subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData[0].binding		= 4u;
		inputData[0].stages			= stages;

		inputData[1].format			= VK_FORMAT_R32_UINT;
		inputData[1].layout			= subgroups::SSBOData::LayoutStd430;
		inputData[1].numElements	= inputData[0].numElements;
		inputData[1].initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData[1].binding		= 5u;
		inputData[1].stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsShuffleTests(tcu::TestContext& testCtx)
{

	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup shuffle category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup shuffle category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup shuffle category tests: framebuffer"));

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

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{

			const string name =
				de::toLower(getOpTypeName(opTypeIndex)) +
				"_" + subgroups::getFormatNameForGLSL(format);

			{
				const CaseDefinition caseDef =
				{
					opTypeIndex,
					VK_SHADER_STAGE_ALL_GRAPHICS,
					format,
					de::SharedPtr<bool>(new bool)
				};
				addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(framebufferGroup.get(), name + "_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "shuffle", "Subgroup shuffle category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
