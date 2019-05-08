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

#include "glcSubgroupsShapeTests.hpp"
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
	ShaderStageFlags	shaderStage;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream				bdy;
	std::string						extension = (OPTYPE_CLUSTERED == caseDef.opType) ?
										"#extension GL_KHR_shader_subgroup_clustered: enable\n" :
										"#extension GL_KHR_shader_subgroup_quad: enable\n";

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	extension += "#extension GL_KHR_shader_subgroup_ballot: enable\n";

	bdy << "  uint tempResult = 0x1u;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << "u)\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    uint modID = gl_SubgroupInvocationID % 32u;\n"
				<< "    switch (gl_SubgroupInvocationID / 32u)\n"
				<< "    {\n"
				<< "    case 0u: contribution.x = 1u << modID; break;\n"
				<< "    case 1u: contribution.y = 1u << modID; break;\n"
				<< "    case 2u: contribution.z = 1u << modID; break;\n"
				<< "    case 3u: contribution.w = 1u << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << "u);\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << "u);\n"
				<< "    for (uint i = 0u; i < " << i << "u; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0u;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  uint[](\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3u)\n"
			<< "  );\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3u;\n"
			<< "  for (uint i = 0u; i < 4u; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << "${VERSION_DECL}\n"
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
		programCollection.add("vert") << glu::VertexSource(vertexSrc.str());
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << "${VERSION_DECL}\n"
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
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.add("geometry") << glu::GeometrySource(geometry.str());
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
	{
		std::ostringstream controlSource;

		controlSource << "${VERSION_DECL}\n"
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
			<< "}\n";

		programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		std::ostringstream evaluationSource;

		evaluationSource << "${VERSION_DECL}\n"
			<< extension
			<< "layout(isolines, equal_spacing, ccw) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  out_color = float(tempResult);\n"
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
	std::string extension = (OPTYPE_CLUSTERED == caseDef.opType) ?
							"#extension GL_KHR_shader_subgroup_clustered: enable\n" :
							"#extension GL_KHR_shader_subgroup_quad: enable\n";

	extension += "#extension GL_KHR_shader_subgroup_ballot: enable\n";

	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1u;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << "u)\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    uint modID = gl_SubgroupInvocationID % 32u;\n"
				<< "    switch (gl_SubgroupInvocationID / 32u)\n"
				<< "    {\n"
				<< "    case 0u: contribution.x = 1u << modID; break;\n"
				<< "    case 1u: contribution.y = 1u << modID; break;\n"
				<< "    case 2u: contribution.z = 1u << modID; break;\n"
				<< "    case 3u: contribution.w = 1u << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << "u);\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << "u);\n"
				<< "    for (uint i = 0u; i < " << i << "u; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0u;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  uint[](\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2u),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3u)\n"
			<< "  );\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3u;\n"
			<< "  for (uint i = 0u; i < 4u; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< extension
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
			<< bdy.str()
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		{
			const string vertex =
				"${VERSION_DECL}\n"
				+ extension +
				"layout(binding = 0, std430) buffer Buffer0\n"
				"{\n"
				"  uint result[];\n"
				"} b0;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  b0.result[gl_VertexID] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"}\n";

			programCollection.add("vert") << glu::VertexSource(vertex);
		}

		{
			const string tesc =
				"${VERSION_DECL}\n"
				+ extension +
				"layout(vertices=1) out;\n"
				"layout(binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"} b1;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  b1.result[gl_PrimitiveID] = 1u;\n"
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
				+ extension +
				"layout(isolines) in;\n"
				"layout(binding = 2, std430) buffer Buffer2\n"
				"{\n"
				"  uint result[];\n"
				"} b2;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = 1u;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";

			programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		}

		{
			const string geometry =
				// version added by addGeometryShadersFromTemplate
				extension +
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(binding = 3, std430) buffer Buffer3\n"
				"{\n"
				"  uint result[];\n"
				"} b3;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  b3.result[gl_PrimitiveIDIn] = tempResult;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
		}

		{
			const string fragment =
				"${VERSION_DECL}\n"
				+ extension +
				"precision highp int;\n"
				"layout(location = 0) out uint result;\n"
				"void main (void)\n"
				"{\n"
				+ bdy.str() +
				"  result = tempResult;\n"
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_CLUSTERED_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that clustered operations are supported!");
		}
	}

	if (OPTYPE_QUAD == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_QUAD_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that quad operations are supported!");
		}
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

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, SHADER_STAGE_TESS_CONTROL_BIT);
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages, SHADER_STAGE_TESS_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
				   "Subgroup feature " +
				   subgroups::getSubgroupFeatureName(SUBGROUP_FEATURE_BASIC_BIT) +
				   " is a required capability!");
	}

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

		if (SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
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
}
}

deqp::TestCaseGroup* createSubgroupsShapeTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup shape category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup shape category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup shape category tests: framebuffer"));

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const std::string op = de::toLower(getOpTypeName(opTypeIndex));

		{
			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);

		}

		{
			const CaseDefinition caseDef =
			{
				opTypeIndex,
				SHADER_STAGE_ALL_GRAPHICS
			};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(),
									op, "",
									supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex]};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),op + "_" + getShaderStageName(caseDef.shaderStage), "",
										supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "shape", "Subgroup shape category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
