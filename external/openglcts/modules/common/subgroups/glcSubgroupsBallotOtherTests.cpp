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

#include "glcSubgroupsBallotOtherTests.hpp"
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
	OPTYPE_INVERSE_BALLOT = 0,
	OPTYPE_BALLOT_BIT_EXTRACT,
	OPTYPE_BALLOT_BIT_COUNT,
	OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_FIND_LSB,
	OPTYPE_BALLOT_FIND_MSB,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return glc::subgroups::check(datas, width, 0xf);
}

static bool checkComputeStage(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 0xf);
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
	ShaderStageFlags	shaderStage;
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
		<< " 0xFFFFFFFF << ((i < 96) ? 0 : (i - 96)))\n"
		<< "#define MAKE_SINGLE_BIT_BALLOT_RESULT(i) uvec4("
		<< "i >= 32 ? 0 : 0x1 << i, "
		<< "i < 32 || i >= 64 ? 0 : 0x1 << (i - 32), "
		<< "i < 64 || i >= 96 ? 0 : 0x1 << (i - 64), "
		<< "i < 96 ? 0 : 0x1 << (i - 96))\n";

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
			bdy << "  tempResult |= gl_SubgroupSize == subgroupBallotBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(MAKE_HIGH_BALLOT_RESULT(gl_SubgroupSize)) ? 0x8 : 0;\n";
			break;
		case OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT:
			bdy << "  uint inclusiveOffset = gl_SubgroupInvocationID + 1;\n"
				<< "  tempResult |= inclusiveOffset == subgroupBallotInclusiveBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotInclusiveBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotInclusiveBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  uvec4 inclusiveUndef = MAKE_HIGH_BALLOT_RESULT(inclusiveOffset);\n"
				<< "  bool undefTerritory = false;\n"
				<< "  for (uint i = 0; i <= 128; i++)\n"
				<< "  {\n"
				<< "    uvec4 iUndef = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    if (iUndef == inclusiveUndef)"
				<< "    {\n"
				<< "      undefTerritory = true;\n"
				<< "    }\n"
				<< "    uint inclusiveBitCount = subgroupBallotInclusiveBitCount(iUndef);\n"
				<< "    if (undefTerritory && (0 != inclusiveBitCount))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "    else if (!undefTerritory && (0 == inclusiveBitCount))\n"
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
				<< "  uvec4 exclusiveUndef = MAKE_HIGH_BALLOT_RESULT(exclusiveOffset);\n"
				<< "  bool undefTerritory = false;\n"
				<< "  for (uint i = 0; i <= 128; i++)\n"
				<< "  {\n"
				<< "    uvec4 iUndef = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    if (iUndef == exclusiveUndef)"
				<< "    {\n"
				<< "      undefTerritory = true;\n"
				<< "    }\n"
				<< "    uint exclusiveBitCount = subgroupBallotExclusiveBitCount(iUndef);\n"
				<< "    if (undefTerritory && (0 != exclusiveBitCount))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x4;\n"
				<< "    }\n"
				<< "    else if (!undefTerritory && (0 == exclusiveBitCount))\n"
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
	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	std::string bdyStr = getBodySource(caseDef);

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
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
		programCollection.add("vert") << glu::VertexSource(vertex.str());
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
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
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.add("geometry") << glu::GeometrySource(geometry.str());
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
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
			<< "}\n";

		programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
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
	std::string bdyStr = getBodySource(caseDef);

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Buffer0\n"
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

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		const string vertex =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(binding = 0, std430) buffer Buffer0\n"
			"{\n"
			"  uint result[];\n"
			"} b0;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  b0.result[gl_VertexID] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices=1) out;\n"
			"layout(binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"} b1;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  b1.result[gl_PrimitiveID] = tempResult;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";

		const string tese =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines) in;\n"
			"layout(binding = 2, std430) buffer Buffer2\n"
			"{\n"
			"  uint result[];\n"
			"} b2;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  b2.result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(binding = 3, std430) buffer Buffer3\n"
			"{\n"
			"  uint result[];\n"
			"} b3;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  b3.result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out uint result;\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.add("vert") << glu::VertexSource(vertex);
		programCollection.add("tesc") << glu::TessellationControlSource(tesc);
		programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
		programCollection.add("fragment") << glu::FragmentSource(fragment);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}
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

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if ((SHADER_STAGE_TESS_CONTROL_BIT | SHADER_STAGE_TESS_EVALUATION_BIT) & caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
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
		return subgroups::makeComputeTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkComputeStage);
	}
	else
	{
		int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

		ShaderStageFlags stages = (ShaderStageFlags)(caseDef.shaderStage & supportedStages);

		if ( SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((ShaderStageFlags)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		return subgroups::allStages(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, stages);
	}
	return tcu::TestStatus::pass("OK");
}
}

deqp::TestCaseGroup* createSubgroupsBallotOtherTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot other category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot other category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot other category tests: framebuffer"));

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const string	op		= de::toLower(getOpTypeName(opTypeIndex));
		{
			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex]};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(), op + "_" + getShaderStageName(caseDef.shaderStage), "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "ballot_other", "Subgroup ballot other category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
