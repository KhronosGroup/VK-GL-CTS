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

#include "glcSubgroupsClusteredTests.hpp"
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
	OPTYPE_CLUSTERED_ADD = 0,
	OPTYPE_CLUSTERED_MUL,
	OPTYPE_CLUSTERED_MIN,
	OPTYPE_CLUSTERED_MAX,
	OPTYPE_CLUSTERED_AND,
	OPTYPE_CLUSTERED_OR,
	OPTYPE_CLUSTERED_XOR,
	OPTYPE_CLUSTERED_LAST
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
		case OPTYPE_CLUSTERED_ADD:
			return "subgroupClusteredAdd";
		case OPTYPE_CLUSTERED_MUL:
			return "subgroupClusteredMul";
		case OPTYPE_CLUSTERED_MIN:
			return "subgroupClusteredMin";
		case OPTYPE_CLUSTERED_MAX:
			return "subgroupClusteredMax";
		case OPTYPE_CLUSTERED_AND:
			return "subgroupClusteredAnd";
		case OPTYPE_CLUSTERED_OR:
			return "subgroupClusteredOr";
		case OPTYPE_CLUSTERED_XOR:
			return "subgroupClusteredXor";
	}
}

std::string getOpTypeOperation(int opType, Format format, std::string lhs, std::string rhs)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED_ADD:
			return lhs + " + " + rhs;
		case OPTYPE_CLUSTERED_MUL:
			return lhs + " * " + rhs;
		case OPTYPE_CLUSTERED_MIN:
			switch (format)
			{
				default:
					return "min(" + lhs + ", " + rhs + ")";
				case FORMAT_R32_SFLOAT:
				case FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : min(" + lhs + ", " + rhs + ")))";
				case FORMAT_R32G32_SFLOAT:
				case FORMAT_R32G32B32_SFLOAT:
				case FORMAT_R32G32B32A32_SFLOAT:
				case FORMAT_R64G64_SFLOAT:
				case FORMAT_R64G64B64_SFLOAT:
				case FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(min(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_MAX:
			switch (format)
			{
				default:
					return "max(" + lhs + ", " + rhs + ")";
				case FORMAT_R32_SFLOAT:
				case FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : max(" + lhs + ", " + rhs + ")))";
				case FORMAT_R32G32_SFLOAT:
				case FORMAT_R32G32B32_SFLOAT:
				case FORMAT_R32G32B32A32_SFLOAT:
				case FORMAT_R64G64_SFLOAT:
				case FORMAT_R64G64B64_SFLOAT:
				case FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(max(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_AND:
			switch (format)
			{
				default:
					return lhs + " & " + rhs;
				case FORMAT_R32_BOOL:
					return lhs + " && " + rhs;
				case FORMAT_R32G32_BOOL:
					return "bvec2(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y)";
				case FORMAT_R32G32B32_BOOL:
					return "bvec3(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z)";
				case FORMAT_R32G32B32A32_BOOL:
					return "bvec4(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z, " + lhs + ".w && " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_OR:
			switch (format)
			{
				default:
					return lhs + " | " + rhs;
				case FORMAT_R32_BOOL:
					return lhs + " || " + rhs;
				case FORMAT_R32G32_BOOL:
					return "bvec2(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y)";
				case FORMAT_R32G32B32_BOOL:
					return "bvec3(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z)";
				case FORMAT_R32G32B32A32_BOOL:
					return "bvec4(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z, " + lhs + ".w || " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_XOR:
			switch (format)
			{
				default:
					return lhs + " ^ " + rhs;
				case FORMAT_R32_BOOL:
					return lhs + " ^^ " + rhs;
				case FORMAT_R32G32_BOOL:
					return "bvec2(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y)";
				case FORMAT_R32G32B32_BOOL:
					return "bvec3(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z)";
				case FORMAT_R32G32B32A32_BOOL:
					return "bvec4(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z, " + lhs + ".w ^^ " + rhs + ".w)";
			}
	}
}

std::string getIdentity(int opType, Format format)
{
	bool isFloat = false;
	bool isInt = false;
	bool isUnsigned = false;

	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			break;
		case FORMAT_R32_SINT:
		case FORMAT_R32G32_SINT:
		case FORMAT_R32G32B32_SINT:
		case FORMAT_R32G32B32A32_SINT:
			isInt = true;
			break;
		case FORMAT_R32_UINT:
		case FORMAT_R32G32_UINT:
		case FORMAT_R32G32B32_UINT:
		case FORMAT_R32G32B32A32_UINT:
			isUnsigned = true;
			break;
		case FORMAT_R32_SFLOAT:
		case FORMAT_R32G32_SFLOAT:
		case FORMAT_R32G32B32_SFLOAT:
		case FORMAT_R32G32B32A32_SFLOAT:
		case FORMAT_R64_SFLOAT:
		case FORMAT_R64G64_SFLOAT:
		case FORMAT_R64G64B64_SFLOAT:
		case FORMAT_R64G64B64A64_SFLOAT:
			isFloat = true;
			break;
		case FORMAT_R32_BOOL:
		case FORMAT_R32G32_BOOL:
		case FORMAT_R32G32B32_BOOL:
		case FORMAT_R32G32B32A32_BOOL:
			break; // bool types are not anything
	}

	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_CLUSTERED_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPTYPE_CLUSTERED_MIN:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0x7f800000))";
			}
			else if (isInt)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0x7fffffff)";
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0xffffffffu)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_CLUSTERED_MAX:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0xff800000))";
			}
			else if (isInt)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0x80000000)";
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0u)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_CLUSTERED_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPTYPE_CLUSTERED_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_XOR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
	}
}

std::string getCompare(int opType, Format format, std::string lhs, std::string rhs)
{
	std::string formatName = subgroups::getFormatNameForGLSL(format);
	switch (format)
	{
		default:
			return "all(equal(" + lhs + ", " + rhs + "))";
		case FORMAT_R32_BOOL:
		case FORMAT_R32_UINT:
		case FORMAT_R32_SINT:
			return "(" + lhs + " == " + rhs + ")";
		case FORMAT_R32_SFLOAT:
		case FORMAT_R64_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.00001)";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case FORMAT_R32G32_SFLOAT:
		case FORMAT_R32G32B32_SFLOAT:
		case FORMAT_R32G32B32A32_SFLOAT:
		case FORMAT_R64G64_SFLOAT:
		case FORMAT_R64G64B64_SFLOAT:
		case FORMAT_R64G64B64A64_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.00001)))";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
	}
}

struct CaseDefinition
{
	int					opType;
	ShaderStageFlags	shaderStage;
	Format				format;
};

std::string getBodySource(CaseDefinition caseDef)
{
	std::ostringstream bdy;
	bdy << "  bool tempResult = true;\n";

	for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
	{
		bdy	<< "  {\n"
			<< "    const uint clusterSize = " << i << "u;\n"
			<< "    if (clusterSize <= gl_SubgroupSize)\n"
			<< "    {\n"
			<< "      " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID], clusterSize);\n"
			<< "      for (uint clusterOffset = 0u; clusterOffset < gl_SubgroupSize; clusterOffset += clusterSize)\n"
			<< "      {\n"
			<< "        " << subgroups::getFormatNameForGLSL(caseDef.format) << " ref = "
			<< getIdentity(caseDef.opType, caseDef.format) << ";\n"
			<< "        for (uint index = clusterOffset; index < (clusterOffset + clusterSize); index++)\n"
			<< "        {\n"
			<< "          if (subgroupBallotBitExtract(mask, index))\n"
			<< "          {\n"
			<< "            ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
			<< "          }\n"
			<< "        }\n"
			<< "        if ((clusterOffset <= gl_SubgroupInvocationID) && (gl_SubgroupInvocationID < (clusterOffset + clusterSize)))\n"
			<< "        {\n"
			<< "          if (!" << getCompare(caseDef.opType, caseDef.format, "ref", "op") << ")\n"
			<< "          {\n"
			<< "            tempResult = false;\n"
			<< "          }\n"
			<< "        }\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
	}
	return bdy.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	std::string bdy = getBodySource(caseDef);

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream				vertexSrc;
		vertexSrc << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy
			<< "  out_color = float(tempResult ? 1 : 0);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";
		programCollection.add("vert") << glu::VertexSource(vertexSrc.str());
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry  << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
			<< bdy
			<< "  out_color = tempResult ? 1.0 : 0.0;\n"
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
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
			<< bdy
			<< "  out_color[gl_InvocationID] = tempResult ? 1.0 : 0.0;\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";

		programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;

		evaluationSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
			<< bdy
			<< "  out_color = tempResult ? 1.0 : 0.0;\n"
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
	std::string bdy = getBodySource(caseDef);

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
			<< bdy
			<< "  result[offset] = tempResult ? 1u : 0u;\n"
			<< "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		{
			const string vertex =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
				+ bdy +
				"  b0.result[gl_VertexID] = tempResult ? 1u : 0u;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"}\n";

			programCollection.add("vert") << glu::VertexSource(vertex);
		}

		{
			const string tesc =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
			+ bdy +
			"  b1.result[gl_PrimitiveID] = tempResult ? 1u : 0u;\n"
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
				"#extension GL_KHR_shader_subgroup_clustered: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(isolines) in;\n"
				"layout(binding = 2, std430) buffer Buffer2\n"
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
				+ bdy +
				"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = tempResult ? 1u : 0u;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";
			programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		}

		{
			const string geometry =
				// version string added by addGeometryShadersFromTemplate
				"#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
				+ bdy +
				"  b3.result[gl_PrimitiveIDIn] = tempResult ? 1u : 0u;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
		}

		{
			const string fragment =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_clustered: enable\n"
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
				+ bdy +
				"  result = tempResult ? 1u : 0u;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_CLUSTERED_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup clustered operations");

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

deqp::TestCaseGroup* createSubgroupsClusteredTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup clustered category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup clustered category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup clustered category tests: framebuffer"));

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT
	};

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

	for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
	{
		const Format format = formats[formatIndex];

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_CLUSTERED_LAST; ++opTypeIndex)
		{
			bool isBool = false;
			bool isFloat = false;

			switch (format)
			{
				default:
					break;
				case FORMAT_R32_SFLOAT:
				case FORMAT_R32G32_SFLOAT:
				case FORMAT_R32G32B32_SFLOAT:
				case FORMAT_R32G32B32A32_SFLOAT:
				case FORMAT_R64_SFLOAT:
				case FORMAT_R64G64_SFLOAT:
				case FORMAT_R64G64B64_SFLOAT:
				case FORMAT_R64G64B64A64_SFLOAT:
					isFloat = true;
					break;
				case FORMAT_R32_BOOL:
				case FORMAT_R32G32_BOOL:
				case FORMAT_R32G32B32_BOOL:
				case FORMAT_R32G32B32A32_BOOL:
					isBool = true;
					break;
			}

			bool isBitwiseOp = false;

			switch (opTypeIndex)
			{
				default:
					break;
				case OPTYPE_CLUSTERED_AND:
				case OPTYPE_CLUSTERED_OR:
				case OPTYPE_CLUSTERED_XOR:
					isBitwiseOp = true;
					break;
			}

			if (isFloat && isBitwiseOp)
			{
				// Skip float with bitwise category.
				continue;
			}

			if (isBool && !isBitwiseOp)
			{
				// Skip bool when its not the bitwise category.
				continue;
			}

			const std::string name = de::toLower(getOpTypeName(opTypeIndex))
				+"_" + subgroups::getFormatNameForGLSL(format);

			{
				const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(), name,
										"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(), name +"_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}
	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "clustered", "Subgroup clustered category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
