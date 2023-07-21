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

#include "glcSubgroupsBuiltinMaskVarTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;

namespace glc
{
namespace subgroups
{

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return check(datas, width, 1);
}

static bool checkComputeStage(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return checkCompute(datas, numWorkgroups, localSize, 1);
}

namespace
{
struct CaseDefinition
{
	std::string			varName;
	ShaderStageFlags	shaderStage;
};
}

std::string subgroupMask (const CaseDefinition& caseDef)
{
	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1u;\n"
		<< "  uint bit        = 0x1u;\n"
		<< "  uint bitCount   = 0x0u;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n"
		<< "  uvec4 var = " << caseDef.varName << ";\n"
		<< "  for (uint i = 0u; i < gl_SubgroupSize; i++)\n"
		<< "  {\n";

	if ("gl_SubgroupEqMask" == caseDef.varName)
	{
		bdy << "    if ((i == gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0u;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupGeMask" == caseDef.varName)
	{
		bdy << "    if ((i >= gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0u;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupGtMask" == caseDef.varName)
	{
		bdy << "    if ((i > gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0u;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupLeMask" == caseDef.varName)
	{
		bdy << "    if ((i <= gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0u;\n"
			<< "    }\n";
	}
	else if ("gl_SubgroupLtMask" == caseDef.varName)
	{
		bdy << "    if ((i < gl_SubgroupInvocationID) ^^ subgroupBallotBitExtract(var, i))\n"
			<< "    {\n"
			<< "      tempResult = 0u;\n"
			<< "    }\n";
	}

	bdy << "  }\n"
		<< "  for (uint i = 0u; i < 32u; i++)\n"
		<< "  {\n"
		<< "    if ((var.x & bit) > 0u)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.y & bit) > 0u)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.z & bit) > 0u)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    if ((var.w & bit) > 0u)\n"
		<< "    {\n"
		<< "      bitCount++;\n"
		<< "    }\n"
		<< "    bit = bit << 1u;\n"
		<< "  }\n"
		<< "  if (subgroupBallotBitCount(var) != bitCount)\n"
		<< "  {\n"
		<< "    tempResult = 0u;\n"
		<< "  }\n";
	return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		const string bdy = subgroupMask(caseDef);
		const string vertexGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out float out_color;\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdy +
			"  out_color = float(tempResult);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.add("vert") << glu::VertexSource(vertexGLSL);
	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		const string bdy = subgroupMask(caseDef);
		const string  evaluationSourceGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"${TESS_EXTENSION}\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) out float out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdy +
			"  out_color = float(tempResult);\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"}\n";
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);
		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
	{
		const string bdy = subgroupMask(caseDef);
		const string  controlSourceGLSL =
			"${VERSION_DECL}\n"
			"${TESS_EXTENSION}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out float out_color[];\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			+ bdy +
			"  out_color[gl_InvocationID] = float(tempResult);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		const string bdy = subgroupMask(caseDef);
		const string geometryGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(location = 0) out float out_color;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdy +
			"  out_color = float(tempResult);\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";
		programCollection.add("geometry") << glu::GeometrySource(geometryGLSL);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}


void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const string bdy = subgroupMask(caseDef);

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Output\n"
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
			<< bdy
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		{
			const string vertex =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(binding = 0, std430) buffer Output0\n"
				"{\n"
				"  uint result[];\n"
				"} b0;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  b0.result[gl_VertexID] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			programCollection.add("vert") << glu::VertexSource(vertex);
		}

		{
			const string tesc =
				"${VERSION_DECL}\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(vertices=1) out;\n"
				"layout(binding = 1, std430) buffer Output1\n"
				"{\n"
				"  uint result[];\n"
				"} b1;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  b1.result[gl_PrimitiveID] = tempResult;\n"
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
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(isolines) in;\n"
				"layout(binding = 2, std430) buffer Output2\n"
				"{\n"
				"  uint result[];\n"
				"} b2;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";

			programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
		}

		{
			const string geometry =
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(binding = 3, std430) buffer Output3\n"
				"{\n"
				"  uint result[];\n"
				"} b3;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
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
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"precision highp int;\n"
				"layout(location = 0) out uint result;\n"
				"void main (void)\n"
				"{\n"
				+ bdy +
				"  result = tempResult;\n"
				"}\n";

			programCollection.add("fragment") << glu::FragmentSource(fragment);
		}

		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");
}

tcu::TestStatus noSSBOtest(Context& context, const CaseDefinition caseDef)
{
	if (!areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return makeVertexFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
	else if ((SHADER_STAGE_TESS_EVALUATION_BIT | SHADER_STAGE_TESS_CONTROL_BIT) & caseDef.shaderStage )
		return makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);

	return makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
				return tcu::TestStatus::fail(
						   "Shader stage " + getShaderStageName(caseDef.shaderStage) +
						   " is required to support subgroup operations!");
		}
		return makeComputeTest(context, FORMAT_R32_UINT, DE_NULL, 0, checkComputeStage);
	}
	else
	{
		int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

		subgroups::ShaderStageFlags stages = (subgroups::ShaderStageFlags)(caseDef.shaderStage & supportedStages);

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
}

deqp::TestCaseGroup* createSubgroupsBuiltinMaskVarTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup builtin mask category	tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup builtin mask category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup builtin mask category tests: framebuffer"));

	const char* const all_stages_vars[] =
	{
		"SubgroupEqMask",
		"SubgroupGeMask",
		"SubgroupGtMask",
		"SubgroupLeMask",
		"SubgroupLtMask",
	};

	const subgroups::ShaderStageFlags stages[] =
	{
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};


	for (int a = 0; a < DE_LENGTH_OF_ARRAY(all_stages_vars); ++a)
	{
		const std::string var = all_stages_vars[a];
		const std::string varLower = de::toLower(var);

		{
			const CaseDefinition caseDef = {"gl_" + var, SHADER_STAGE_ALL_GRAPHICS};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition caseDef = {"gl_" + var, SHADER_STAGE_COMPUTE_BIT};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(),
										varLower, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition caseDef = {"gl_" + var, stages[stageIndex]};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),
						varLower + "_" +
						getShaderStageName(caseDef.shaderStage), "",
						supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "builtin_mask_var", "Subgroup builtin mask variable tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // glc
