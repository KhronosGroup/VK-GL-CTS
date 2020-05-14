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

#include "vktSubgroupsShapeTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
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

enum OpType
{
	OPTYPE_CLUSTERED = 0,
	OPTYPE_QUAD,
	OPTYPE_LAST
};

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED:
			return "clustered";
		case OPTYPE_QUAD:
			return "quad";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	std::ostringstream				bdy;
	std::string						extension = (OPTYPE_CLUSTERED == caseDef.opType) ?
										"#extension GL_KHR_shader_subgroup_clustered: enable\n" :
										"#extension GL_KHR_shader_subgroup_quad: enable\n";

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	extension += "#extension GL_KHR_shader_subgroup_ballot: enable\n";

	bdy << "  uint tempResult = 0x1;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << ")\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    const uint modID = gl_SubgroupInvocationID % 32;\n"
				<< "    switch (gl_SubgroupInvocationID / 32)\n"
				<< "    {\n"
				<< "    case 0: contribution.x = 1 << modID; break;\n"
				<< "    case 1: contribution.y = 1 << modID; break;\n"
				<< "    case 2: contribution.z = 1 << modID; break;\n"
				<< "    case 3: contribution.w = 1 << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << ");\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << ");\n"
				<< "    for (uint i = 0; i < " << i << "; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  {\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3)\n"
			<< "  };\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3;\n"
			<< "  for (uint i = 0; i < 4; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extension
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result = float(tempResult);\n"
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
			<< extension
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
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
			<< extension
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< bdy.str()
			<< "  out_color[gl_InvocationID] = float(tempResult);\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;

		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< extension
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  out_color = float(tempResult);\n"
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
	std::string extension = (OPTYPE_CLUSTERED == caseDef.opType) ?
							"#extension GL_KHR_shader_subgroup_clustered: enable\n" :
							"#extension GL_KHR_shader_subgroup_quad: enable\n";

	extension += "#extension GL_KHR_shader_subgroup_ballot: enable\n";

	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << ")\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    const uint modID = gl_SubgroupInvocationID % 32;\n"
				<< "    switch (gl_SubgroupInvocationID / 32)\n"
				<< "    {\n"
				<< "    case 0: contribution.x = 1 << modID; break;\n"
				<< "    case 1: contribution.y = 1 << modID; break;\n"
				<< "    case 2: contribution.z = 1 << modID; break;\n"
				<< "    case 3: contribution.w = 1 << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << ");\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << ");\n"
				<< "    for (uint i = 0; i < " << i << "; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  {\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3)\n"
			<< "  };\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3;\n"
			<< "  for (uint i = 0; i < 4; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< extension
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
			<< bdy.str()
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		{
			const string vertex =
				"#version 450\n"
				+ extension +
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result[gl_VertexIndex] = tempResult;\n"
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
				"#version 450\n"
				+ extension +
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result[gl_PrimitiveID] = 1;\n"
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
				"#version 450\n"
				+ extension +
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = 1;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				+ (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "") +
				"}\n";

			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				"#version 450\n"
				+ extension +
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result[gl_PrimitiveIDIn] = tempResult;\n"
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
				"#version 450\n"
				+ extension +
				"layout(location = 0) out uint result;\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result = tempResult;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_CLUSTERED_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that clustered operations are supported!");
		}
	}

	if (OPTYPE_QUAD == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_QUAD_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that quad operations are supported!");
		}
	}

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

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
				   "Subgroup feature " +
				   subgroups::getShaderStageName(VK_SUBGROUP_FEATURE_BASIC_BIT) +
				   " is a required capability!");
	}

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

		if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
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
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsShapeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup shape category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup shape category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup shape category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const std::string op = de::toLower(getOpTypeName(opTypeIndex));

		{
			const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool)};
			addFunctionCaseWithPrograms(computeGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef =
			{
				opTypeIndex,
				VK_SHADER_STAGE_ALL_GRAPHICS,
				de::SharedPtr<bool>(new bool)
			};
			addFunctionCaseWithPrograms(graphicGroup.get(),
									op, "",
									supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], de::SharedPtr<bool>(new bool)};
			addFunctionCaseWithPrograms(framebufferGroup.get(),op + "_" + getShaderStageName(caseDef.shaderStage), "",
										supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "shape", "Subgroup shape category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
