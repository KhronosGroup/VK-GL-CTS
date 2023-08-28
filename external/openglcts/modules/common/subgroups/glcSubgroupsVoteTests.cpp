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

#include "glcSubgroupsVoteTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>
#include "tcuStringTemplate.hpp"

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
	OPTYPE_ALL = 0,
	OPTYPE_ANY,
	OPTYPE_ALLEQUAL,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return glc::subgroups::check(datas, width, 0x1F);
}

static bool checkFragmentPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y);
			deUint32 val = data[ndx] & 0x1F;

			if (data[ndx] & 0x40) //Helper fragment shader invocation was executed
			{
				if(val != 0x1F)
					return false;
			}
			else //Helper fragment shader invocation was not executed yet
			{
				if (val != 0x1E)
					return false;
			}
		}
	}
	return true;
}

static bool checkComputeStage(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 0x1F);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ALL:
			return "subgroupAll";
		case OPTYPE_ANY:
			return "subgroupAny";
		case OPTYPE_ALLEQUAL:
			return "subgroupAllEqual";
	}
}

struct CaseDefinition
{
	int					opType;
	ShaderStageFlags	shaderStage;
	Format				format;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool formatIsBoolean =
		FORMAT_R32_BOOL == caseDef.format || FORMAT_R32G32_BOOL == caseDef.format || FORMAT_R32G32B32_BOOL == caseDef.format || FORMAT_R32G32B32A32_BOOL == caseDef.format;

	if (SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
		subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string vertex	= "${VERSION_DECL}\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
			"  gl_Position = vec4(uv * 4.0f -2.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.add("vert") << glu::VertexSource(vertex);
	}
	else if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	const string source =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result = " + getOpTypeName(caseDef.opType) +
			"(true) ? 0x1u : 0u;\n"
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0u : 0x1Au;\n"
			"  result |= 0x4u;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0u : 0x1Au;\n"
				"  result |= 0x4u;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect())\n;" : "(12.0 * float(data[gl_SubgroupInvocationID]) + float(gl_SubgroupInvocationID));\n") +
				"  result = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0u : 0x2u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8u : 0x0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueNoEqual) ? 0x0u : 0x10u;\n"
				"  if (subgroupElect()) result |= 0x2u | 0x10u;\n"
		: "";

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(binding = 0, std140) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.add("vert") << glu::VertexSource(vertexSrc.str());
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< source
			<< "  out_color = float(result);\n"
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
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(binding = 0, std140) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< source
			<< "  out_color[gl_InvocationID] = float(result);"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";

		programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;
		evaluationSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "${TESS_EXTENSION}\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uint result;\n"
			<< "  highp uint offset = uint(gl_PrimitiveID) * 2u + uint(gl_TessCoord.x + 0.5);\n"
			<< source
			<< "  out_color = float(result);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str());
	}
	else if (SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string sourceFragment =
		(OPTYPE_ALL == caseDef.opType) ?
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(!gl_HelperInvocation) ? 0x0u : 0x1u;\n"
			"  result |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0u : 0x1Au;\n"
			"  result |= 0x4u;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_HelperInvocation) ? 0x1u : 0x0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0u : 0x1Au;\n"
				"  result |= 0x4u;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + gl_FragCoord.x * float(gl_SubgroupInvocationID));\n") +
				"  result |= " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x10u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0u : 0x2u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8u : 0x0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(gl_HelperInvocation) ? 0x0u : 0x1u;\n"
				"  if (subgroupElect()) result |= 0x2u | 0x10u;\n"
		: "";

		std::ostringstream fragmentSource;
		fragmentSource << "${VERSION_DECL}\n"
		<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
		<< "precision highp float;\n"
		<< "layout(location = 0) out uint out_color;\n"
		<< "layout(binding = 0, std140) uniform Buffer1\n"
		<< "{\n"
		<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
		<< "};\n"
		<< ""
		<< "void main()\n"
		<< "{\n"
		<< "  uint result = 0u;\n"
		<< "  if (dFdx(float(gl_SubgroupInvocationID) * gl_FragCoord.x * gl_FragCoord.y) - dFdy(float(gl_SubgroupInvocationID) * gl_FragCoord.x * gl_FragCoord.y) > 0.0f)\n"
		<< "  {\n"
		<< "    result |= 0x20u;\n" // to be sure that compiler doesn't remove dFdx and dFdy executions
		<< "  }\n"
		<< "  bool helper = subgroupAny(gl_HelperInvocation);\n"
		<< "  if (helper)\n"
		<< "  {\n"
		<< "    result |= 0x40u;\n"
		<< "  }\n"
		<< sourceFragment
		<< "  out_color = result;\n"
		<< "}\n";

		programCollection.add("fragment") << glu::FragmentSource(fragmentSource.str());
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool formatIsBoolean =
		FORMAT_R32_BOOL == caseDef.format || FORMAT_R32G32_BOOL == caseDef.format || FORMAT_R32G32B32_BOOL == caseDef.format || FORMAT_R32G32B32A32_BOOL == caseDef.format;
	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_vote: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n";
		if (OPTYPE_ALL == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1u : 0u;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0u : 0x1Au;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] > 0u) ? 0x4u : 0u;\n";
		}
		else if (OPTYPE_ANY == caseDef.opType)
		{
			src << "  result[offset] = " << getOpTypeName(caseDef.opType)
				<< "(true) ? 0x1u : 0u;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(false) ? 0u : 0x1Au;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[gl_SubgroupInvocationID] == data[0]) ? 0x4u : 0u;\n";
		}

		else if (OPTYPE_ALLEQUAL == caseDef.opType)
		{
			src << "  " << subgroups::getFormatNameForGLSL(caseDef.format) <<" valueEqual = " << subgroups::getFormatNameForGLSL(caseDef.format) << "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n"
				<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) <<" valueNoEqual = " << subgroups::getFormatNameForGLSL(caseDef.format) << (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + float(offset));\n")
				<<"  result[offset] = " << getOpTypeName(caseDef.opType) << "("
				<< subgroups::getFormatNameForGLSL(caseDef.format) << "(1)) ? 0x1u : 0x0u;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(gl_SubgroupInvocationID) ? 0x0u : 0x2u;\n"
				<< "  result[offset] |= " << getOpTypeName(caseDef.opType)
				<< "(data[0]) ? 0x4u : 0x0u;\n"
				<< "  result[offset] |= "<< getOpTypeName(caseDef.opType)
				<< "(valueEqual) ? 0x8u : 0x0u;\n"
				<< "  result[offset] |= "<< getOpTypeName(caseDef.opType)
				<< "(valueNoEqual) ? 0x0u : 0x10u;\n"
				<< "  if (subgroupElect()) result[offset] |= 0x2u | 0x10u;\n";
		}

		src << "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		const string source =
		(OPTYPE_ALL == caseDef.opType) ?
			"  b${SSBO1}.result[offset] = " + getOpTypeName(caseDef.opType) +
			"(true) ? 0x1u : 0u;\n"
			"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
			"(false) ? 0u : 0x1Au;\n"
			"  b${SSBO1}.result[offset] |= 0x4u;\n"
		: (OPTYPE_ANY == caseDef.opType) ?
				"  b${SSBO1}.result[offset] = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1u : 0u;\n"
				"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0u : 0x1Au;\n"
				"  b${SSBO1}.result[offset] |= 0x4u;\n"
		: (OPTYPE_ALLEQUAL == caseDef.opType) ?
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + float(gl_SubgroupInvocationID));\n") +
				"  b${SSBO1}.result[offset] = " + getOpTypeName(caseDef.opType) + "("
				+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1u : 0u;\n"
				"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(gl_SubgroupInvocationID) ? 0u : 0x2u;\n"
				"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(data[0]) ? 0x4u : 0u;\n"
				"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(valueEqual) ? 0x8u : 0x0u;\n"
				"  b${SSBO1}.result[offset] |= " + getOpTypeName(caseDef.opType) +
				"(valueNoEqual) ? 0x0u : 0x10u;\n"
				"  if (subgroupElect()) b${SSBO1}.result[offset] |= 0x2u | 0x10u;\n"
		: "";

		tcu::StringTemplate sourceTemplate(source);

		const string formatString = subgroups::getFormatNameForGLSL(caseDef.format);

		{
			map<string, string> bufferNameMapping;
			bufferNameMapping.insert(pair<string, string>("SSBO1", "0"));

			const string vertex =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(binding = 0, std430) buffer Buffer0\n"
				"{\n"
				"  uint result[];\n"
				"} b0;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp int offset = gl_VertexID;\n"
				+ sourceTemplate.specialize(bufferNameMapping) +
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			programCollection.add("vert") << glu::VertexSource(vertex);
		}

		{
			map<string, string> bufferNameMapping;
			bufferNameMapping.insert(pair<string, string>("SSBO1", "1"));

			const string tesc =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(vertices=1) out;\n"
				"layout(binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"} b1;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp int offset = gl_PrimitiveID;\n"
				+ sourceTemplate.specialize(bufferNameMapping) +
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
			map<string, string> bufferNameMapping;
			bufferNameMapping.insert(pair<string, string>("SSBO1", "2"));

			const string tese =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(isolines) in;\n"
				"layout(binding = 2, std430) buffer Buffer2\n"
				"{\n"
				"  uint result[];\n"
				"} b2;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp uint offset = uint(gl_PrimitiveID * 2) + uint(gl_TessCoord.x + 0.5);\n"
				+ sourceTemplate.specialize(bufferNameMapping) +
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";

			programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		}

		{
			map<string, string> bufferNameMapping;
			bufferNameMapping.insert(pair<string, string>("SSBO1", "3"));

			const string geometry =
				// version string added by addGeometryShadersFromTemplate
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(binding = 3, std430) buffer Buffer3\n"
				"{\n"
				"  uint result[];\n"
				"} b3;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  highp int offset = gl_PrimitiveIDIn;\n"
				+ sourceTemplate.specialize(bufferNameMapping) +
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
		}

		{
			const string sourceFragment =
			(OPTYPE_ALL == caseDef.opType) ?
				"  result = " + getOpTypeName(caseDef.opType) +
				"(true) ? 0x1u : 0u;\n"
				"  result |= " + getOpTypeName(caseDef.opType) +
				"(false) ? 0u : 0x1Au;\n"
				"  result |= 0x4u;\n"
			: (OPTYPE_ANY == caseDef.opType) ?
					"  result = " + getOpTypeName(caseDef.opType) +
					"(true) ? 0x1u : 0u;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(false) ? 0u : 0x1Au;\n"
					"  result |= 0x4u;\n"
			: (OPTYPE_ALLEQUAL == caseDef.opType) ?
					"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + "(1.25 * float(data[gl_SubgroupInvocationID]) + 5.0);\n" +
					"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " valueNoEqual = " + subgroups::getFormatNameForGLSL(caseDef.format) + (formatIsBoolean ? "(subgroupElect());\n" : "(12.0 * float(data[gl_SubgroupInvocationID]) + gl_FragCoord.x * float(gl_SubgroupInvocationID));\n") +
					"  result = " + getOpTypeName(caseDef.opType) + "("
					+ subgroups::getFormatNameForGLSL(caseDef.format) + "(1)) ? 0x1u : 0u;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(gl_SubgroupInvocationID) ? 0u : 0x2u;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(data[0]) ? 0x4u : 0u;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(valueEqual) ? 0x8u : 0x0u;\n"
					"  result |= " + getOpTypeName(caseDef.opType) +
					"(valueNoEqual) ? 0x0u : 0x10u;\n"
					"  if (subgroupElect()) result |= 0x2u | 0x10u;\n"
			: "";
			const string fragment =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_vote: enable\n"
				"precision highp float;\n"
				"layout(location = 0) out uint result;\n"
				"layout(binding = 4, std430) readonly buffer Buffer4\n"
				"{\n"
				"  " + formatString + " data[];\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				+ sourceFragment +
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, subgroups::SUBGROUP_FEATURE_VOTE_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup vote operations");
	}

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
	inputData.initializeType = OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_CONTROL_BIT);
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_EVALUATION_BIT);
	else if (SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		return subgroups::makeFragmentFrameBufferTest(context, FORMAT_R32_UINT, &inputData, 1, checkFragmentPipelineStages);
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
		inputData.initializeType = OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;
		inputData.binding = 1u;

		return subgroups::makeComputeTest(context, FORMAT_R32_UINT, &inputData,
										  1, checkComputeStage);
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
		inputData.initializeType	= OPTYPE_ALLEQUAL == caseDef.opType ? subgroups::SSBOData::InitializeZero : subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}

} // namespace

deqp::TestCaseGroup* createSubgroupsVoteTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	de::MovePtr<deqp::TestCaseGroup> fragHelperGroup(new deqp::TestCaseGroup(
		testCtx, "frag_helper", "Subgroup arithmetic category tests: fragment helper invocation"));

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
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

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			// Skip the typed tests for all but subgroupAllEqual()
			if ((FORMAT_R32_UINT != format) && (OPTYPE_ALLEQUAL != opTypeIndex))
			{
				continue;
			}

			const std::string op = de::toLower(getOpTypeName(opTypeIndex));

			{
				const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(),
											op + "_" + subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),
							op + "_" +
							subgroups::getFormatNameForGLSL(format)
							+ "_" + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}

			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_FRAGMENT_BIT, format};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(fragHelperGroup.get(),
						op + "_" +
						subgroups::getFormatNameForGLSL(format)
						+ "_" + getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "vote", "Subgroup vote category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(fragHelperGroup.release());

	return group.release();
}

} // subgroups
} // glc
