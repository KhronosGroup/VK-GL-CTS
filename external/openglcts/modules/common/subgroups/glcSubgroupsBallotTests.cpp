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

#include "glcSubgroupsBallotTests.hpp"
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
	return glc::subgroups::check(datas, width, 0x7);
}

static bool checkComputeStage(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 0x7);
}

struct CaseDefinition
{
	glc::subgroups::ShaderStageFlags	shaderStage;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream				subgroupSizeStr;
	subgroupSizeStr << subgroups::maxSupportedSubgroupSize();

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		const string vertexGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(binding = 0, std140) uniform Buffer1\n"
			"{\n"
			"  uint data[" + subgroupSizeStr.str() + "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.add("vert") << glu::VertexSource(vertexGLSL);
	}
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		const string geometryGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(binding = 0, std140) uniform Buffer1\n"
			"{\n"
			"  uint data[" + subgroupSizeStr.str() + "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";
		programCollection.add("geometry") << glu::GeometrySource(geometryGLSL);
	}
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
	{
		const string controlSourceGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out float out_color[];\n"
			"layout(binding = 0, std140) uniform Buffer1\n"
			"{\n"
			"  uint data[" + subgroupSizeStr.str() + "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  out_color[gl_InvocationID] = float(tempResult);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);
		subgroups::setTesEvalShaderFrameBuffer(programCollection);

	}
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
	{
		const string evaluationSourceGLSL =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(binding = 0, std140) uniform Buffer1\n"
			"{\n"
			"  uint data[" + subgroupSizeStr.str() + "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"}\n";
		programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}


void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< subgroups::getSharedMemoryBallotHelper()
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uint tempResult = 0u;\n"
			<< "  tempResult |= sharedMemoryBallot(true) == subgroupBallot(true) ? 0x1u : 0u;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			<< "  tempResult |= sharedMemoryBallot(bData) == subgroupBallot(bData) ? 0x2u : 0u;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.add("comp") << glu::ComputeSource(src.str());
	}
	else
	{
		const string vertex =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(binding = 0, std430) buffer Buffer0\n"
			"{\n"
			"  uint result[];\n"
			"} b0;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  b0.result[gl_VertexID] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices=1) out;\n"
			"layout(binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"} b1;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  b1.result[gl_PrimitiveID] = tempResult;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";

		const string tese =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines) in;\n"
			"layout(binding = 2, std430) buffer Buffer2\n"
			"{\n"
			"  uint result[];\n"
			"} b2;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			// version string added by addGeometryShadersFromTemplate
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(binding = 3, std430) buffer Buffer3\n"
			"{\n"
			"  uint result[];\n"
			"} b3;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
			"  b3.result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"precision highp int;\n"
			"layout(location = 0) out uint result;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1u : 0u;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0u;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2u : 0u;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4u : 0u;\n"
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

	subgroups::SSBOData inputData[1];
	inputData[0].format = FORMAT_R32_UINT;
	inputData[0].layout = subgroups::SSBOData::LayoutStd140;
	inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
	inputData[0].binding = 0u;

	if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_CONTROL_BIT);
	else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, SHADER_STAGE_TESS_EVALUATION_BIT);
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
		subgroups::SSBOData inputData[1];
		inputData[0].format = FORMAT_R32_UINT;
		inputData[0].layout = subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
		inputData[0].binding = 1u;

		return subgroups::makeComputeTest(context, FORMAT_R32_UINT, inputData, 1, checkComputeStage);
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

		subgroups::SSBOData inputData;
		inputData.format			= FORMAT_R32_UINT;
		inputData.layout            = subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

deqp::TestCaseGroup* createSubgroupsBallotTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot category tests: framebuffer"));

	const ShaderStageFlags stages[] =
	{
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
		SHADER_STAGE_VERTEX_BIT
	};

	{
		const CaseDefinition caseDef = {SHADER_STAGE_COMPUTE_BIT};
		SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(),
									getShaderStageName(caseDef.shaderStage), "",
									supportedCheck, initPrograms, test, caseDef);
	}

	{
			const CaseDefinition caseDef = {SHADER_STAGE_ALL_GRAPHICS};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(), "graphic", "", supportedCheck, initPrograms, test, caseDef);
	}

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const CaseDefinition caseDef = {stages[stageIndex]};
		SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(), getShaderStageName(caseDef.shaderStage), "",
					supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "ballot", "Subgroup ballot category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
