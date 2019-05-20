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

#include "vktSubgroupsQuadTests.hpp"
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
	OPTYPE_QUAD_BROADCAST = 0,
	OPTYPE_QUAD_SWAP_HORIZONTAL,
	OPTYPE_QUAD_SWAP_VERTICAL,
	OPTYPE_QUAD_SWAP_DIAGONAL,
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
		case OPTYPE_QUAD_BROADCAST:
			return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:
			return "subgroupQuadSwapHorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:
			return "subgroupQuadSwapVertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:
			return "subgroupQuadSwapDiagonal";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	int					direction;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	std::string			swapTable[OPTYPE_LAST];

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	swapTable[OPTYPE_QUAD_BROADCAST] = "";
	swapTable[OPTYPE_QUAD_SWAP_HORIZONTAL] = "  const uint swapTable[4] = {1, 0, 3, 2};\n";
	swapTable[OPTYPE_QUAD_SWAP_VERTICAL] = "  const uint swapTable[4] = {2, 3, 0, 1};\n";
	swapTable[OPTYPE_QUAD_SWAP_DIAGONAL] = "  const uint swapTable[4] = {3, 2, 1, 0};\n";

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream	vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType];

		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			vertexSrc << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			vertexSrc << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}

		vertexSrc << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    result = (op == data[otherID]) ? 1.0f : 0.0f;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result = 1.0f;\n" // Invocation we read from was inactive, so we can't verify results!
			<< "  }\n"
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
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"

			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType];

		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			geometry << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			geometry << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}

		geometry << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    out_color = (op == data[otherID]) ? 1.0 : 0.0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    out_color = 1.0;\n" // Invocation we read from was inactive, so we can't verify results!
			<< "  }\n"
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
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType];

		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			controlSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			controlSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}

		controlSource << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    out_color[gl_InvocationID] = (op == data[otherID]) ? 1.0 : 0.0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    out_color[gl_InvocationID] = 1.0; \n"// Invocation we read from was inactive, so we can't verify results!
			<< "  }\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType];

		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			evaluationSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			evaluationSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}

		evaluationSource << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    out_color = (op == data[otherID]) ? 1.0 : 0.0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    out_color = 1.0;\n" // Invocation we read from was inactive, so we can't verify results!
			<< "  }\n"
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
	std::string swapTable[OPTYPE_LAST];
	swapTable[OPTYPE_QUAD_BROADCAST] = "";
	swapTable[OPTYPE_QUAD_SWAP_HORIZONTAL] = "  const uint swapTable[4] = {1, 0, 3, 2};\n";
	swapTable[OPTYPE_QUAD_SWAP_VERTICAL] = "  const uint swapTable[4] = {2, 3, 0, 1};\n";
	swapTable[OPTYPE_QUAD_SWAP_DIAGONAL] = "  const uint swapTable[4] = {3, 2, 1, 0};\n";

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType];


		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}

		src << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    result[offset] = (op == data[otherID]) ? 1 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[offset] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		std::ostringstream src;
		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << ");\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << caseDef.direction << ";\n";
		}
		else
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n";
		}
		const string sourceType = src.str();

		{
			const string vertex =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ swapTable[caseDef.opType]
				+ sourceType +
				"  if (subgroupBallotBitExtract(mask, otherID))\n"
				"  {\n"
				"    result[gl_VertexIndex] = (op == data[otherID]) ? 1 : 0;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result[gl_VertexIndex] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
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
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ swapTable[caseDef.opType]
				+ sourceType +
				"  if (subgroupBallotBitExtract(mask, otherID))\n"
				"  {\n"
				"    result[gl_PrimitiveID] = (op == data[otherID]) ? 1 : 0;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result[gl_PrimitiveID] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";
			programCollection.glslSources.add("tesc")
					<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string tese =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430)  buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ swapTable[caseDef.opType]
				+ sourceType +
				"  if (subgroupBallotBitExtract(mask, otherID))\n"
				"  {\n"
				"    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = (op == data[otherID]) ? 1 : 0;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";
			programCollection.glslSources.add("tese")
					<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const string geometry =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ swapTable[caseDef.opType]
				+ sourceType +
				"  if (subgroupBallotBitExtract(mask, otherID))\n"
				"  {\n"
				"    result[gl_PrimitiveIDIn] = (op == data[otherID]) ? 1 : 0;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result[gl_PrimitiveIDIn] = 1; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const string fragment =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(location = 0) out uint result;\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ swapTable[caseDef.opType]
				+ sourceType +
				"  if (subgroupBallotBitExtract(mask, otherID))\n"
				"  {\n"
				"    result = (op == data[otherID]) ? 1 : 0;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result = 1; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_QUAD_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations");


	if (subgroups::isDoubleFormat(caseDef.format) &&
			!subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
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

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
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
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkCompute);
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

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
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
tcu::TestCaseGroup* createSubgroupsQuadTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	const VkFormat formats[] =
	{
		VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_SFLOAT, VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT, VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED,
	};

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int direction = 0; direction < 4; ++direction)
	{
		for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
		{
			const VkFormat format = formats[formatIndex];

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const std::string op = de::toLower(getOpTypeName(opTypeIndex));
				std::ostringstream name;
				name << de::toLower(op);

				if (OPTYPE_QUAD_BROADCAST == opTypeIndex)
				{
					name << "_" << direction;
				}
				else
				{
					if (0 != direction)
					{
						// We don't need direction for swap operations.
						continue;
					}
				}

				name << "_" << subgroups::getFormatNameForGLSL(format);

				{
					const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, direction, de::SharedPtr<bool>(new bool)};
					addFunctionCaseWithPrograms(computeGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
				}

				{
					const CaseDefinition caseDef =
					{
						opTypeIndex,
						VK_SHADER_STAGE_ALL_GRAPHICS,
						format,
						direction,
						de::SharedPtr<bool>(new bool)
					};
					addFunctionCaseWithPrograms(graphicGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
				}
				for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
				{
					const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, direction, de::SharedPtr<bool>(new bool)};
					addFunctionCaseWithPrograms(framebufferGroup.get(), name.str()+"_"+ getShaderStageName(caseDef.shaderStage), "",
												supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}

			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "quad", "Subgroup quad category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // vkt
