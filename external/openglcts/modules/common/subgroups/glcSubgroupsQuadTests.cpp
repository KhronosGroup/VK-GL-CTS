/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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

#include "glcSubgroupsQuadTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;

namespace glc
{
namespace subgroups
{
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
	return glc::subgroups::check(datas, width, 1);
}

static bool checkComputeStage(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
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
	ShaderStageFlags	shaderStage;
	Format				format;
	int					direction;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string			swapTable[OPTYPE_LAST];

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	swapTable[OPTYPE_QUAD_BROADCAST] = "";
	swapTable[OPTYPE_QUAD_SWAP_HORIZONTAL] = "  const uint swapTable[4] = uint[](1u, 0u, 3u, 2u);\n";
	swapTable[OPTYPE_QUAD_SWAP_VERTICAL] = "  const uint swapTable[4] = uint[](2u, 3u, 0u, 1u);\n";
	swapTable[OPTYPE_QUAD_SWAP_DIAGONAL] = "  const uint swapTable[4] = uint[](3u, 2u, 1u, 0u);\n";

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream	vertexSrc;
		vertexSrc << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
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
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			vertexSrc << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
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
		programCollection.add("vert") << glu::VertexSource(vertexSrc.str());
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
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
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			geometry << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
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
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.add("geometry") << glu::GeometrySource(geometry.str());
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
	{
		std::ostringstream controlSource;

		controlSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
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
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			controlSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
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

		programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		ostringstream evaluationSource;
		evaluationSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
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
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			evaluationSource << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
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
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str());
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
	swapTable[OPTYPE_QUAD_SWAP_HORIZONTAL] = "  const uint swapTable[4] = uint[](1u, 0u, 3u, 2u);\n";
	swapTable[OPTYPE_QUAD_SWAP_VERTICAL] = "  const uint swapTable[4] = uint[](2u, 3u, 0u, 1u);\n";
	swapTable[OPTYPE_QUAD_SWAP_DIAGONAL] = "  const uint swapTable[4] = uint[](3u, 2u, 1u, 0u);\n";

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_quad: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Buffer0\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(binding = 1, std430) buffer Buffer1\n"
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
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
		}

		src << "  if (subgroupBallotBitExtract(mask, otherID))\n"
			<< "  {\n"
			<< "    result[offset] = (op == data[otherID]) ? 1u : 0u;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    result[offset] = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
			<< "  }\n"
			<< "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		std::ostringstream src;
		if (OPTYPE_QUAD_BROADCAST == caseDef.opType)
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID], " << caseDef.direction << "u);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + " << caseDef.direction << "u;\n";
		}
		else
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
				<< getOpTypeName(caseDef.opType) << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3u) + swapTable[gl_SubgroupInvocationID & 0x3u];\n";
		}
		const string sourceType = src.str();

		{
			const string vertex =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(binding = 0, std430) buffer Buffer0\n"
				"{\n"
				"  uint result[];\n"
				"} b0;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
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
				"    b0.result[gl_VertexID] = (op == data[otherID]) ? 1u : 0u;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    b0.result[gl_VertexID] = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"}\n";
			programCollection.add("vert") << glu::VertexSource(vertex);
		}

		{
			const string tesc =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(vertices=1) out;\n"
				"layout(binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"} b1;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
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
				"    b1.result[gl_PrimitiveID] = (op == data[otherID]) ? 1u : 0u;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    b1.result[gl_PrimitiveID] = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";
			programCollection.add("tesc") << glu::TessellationControlSource(tesc);
		}

		{
			const string tese =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(isolines) in;\n"
				"layout(binding = 2, std430)  buffer Buffer2\n"
				"{\n"
				"  uint result[];\n"
				"} b2;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
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
				"    b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = (op == data[otherID]) ? 1u : 0u;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";
			programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		}

		{
			const string geometry =
				// version added by addGeometryShadersFromTemplate
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(binding = 3, std430) buffer Buffer3\n"
				"{\n"
				"  uint result[];\n"
				"} b3;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
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
				"    b3.result[gl_PrimitiveIDIn] = (op == data[otherID]) ? 1u : 0u;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    b3.result[gl_PrimitiveIDIn] = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
		}

		{
			const string fragment =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_quad: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"precision highp int;\n"
				"precision highp float;\n"
				"layout(location = 0) out uint result;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
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
				"    result = (op == data[otherID]) ? 1u : 0u;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    result = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
				"  }\n"
				"}\n";
			programCollection.add("fragment") << glu::FragmentSource(fragment);
		}
		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_QUAD_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations");


	if (subgroups::isDoubleFormat(caseDef.format) &&
			!subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
	}
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
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;
	inputData.binding = 0u;

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_CONTROL_BIT);
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
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
		inputData.binding = 1u;

		return subgroups::makeComputeTest(context, FORMAT_R32_UINT, &inputData, 1, checkComputeStage);
	}
	else
	{
		int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

		ShaderStageFlags stages = (ShaderStageFlags)(caseDef.shaderStage & supportedStages);

		if (SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((ShaderStageFlags)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

deqp::TestCaseGroup* createSubgroupsQuadTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	const Format formats[] =
	{
		FORMAT_R32_SINT, FORMAT_R32G32_SINT, FORMAT_R32G32B32_SINT,
		FORMAT_R32G32B32A32_SINT, FORMAT_R32_UINT, FORMAT_R32G32_UINT,
		FORMAT_R32G32B32_UINT, FORMAT_R32G32B32A32_UINT,
		FORMAT_R32_SFLOAT, FORMAT_R32G32_SFLOAT,
		FORMAT_R32G32B32_SFLOAT, FORMAT_R32G32B32A32_SFLOAT,
		FORMAT_R64_SFLOAT, FORMAT_R64G64_SFLOAT,
		FORMAT_R64G64B64_SFLOAT, FORMAT_R64G64B64A64_SFLOAT,
		FORMAT_R32_BOOL, FORMAT_R32G32_BOOL,
		FORMAT_R32G32B32_BOOL, FORMAT_R32G32B32A32_BOOL,
	};

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int direction = 0; direction < 4; ++direction)
	{
		for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
		{
			const Format format = formats[formatIndex];

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
					const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT, format, direction};
					SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
				}

				{
					const CaseDefinition caseDef =
					{
						opTypeIndex,
						SHADER_STAGE_ALL_GRAPHICS,
						format,
						direction
					};
					SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
				}
				for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
				{
					const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, direction};
					SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(), name.str()+"_"+ getShaderStageName(caseDef.shaderStage), "",
												supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}

			}
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "quad", "Subgroup quad category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // glc
