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

#include "glcSubgroupsBallotBroadcastTests.hpp"
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
	OPTYPE_BROADCAST = 0,
	OPTYPE_BROADCAST_FIRST,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return glc::subgroups::check(datas, width, 3);
}

static bool checkComputeStages(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 3);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_BROADCAST:
			return "subgroupBroadcast";
		case OPTYPE_BROADCAST_FIRST:
			return "subgroupBroadcastFirst";
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

	bdy << "  uvec4 mask = subgroupBallot(true);\n";
	bdy << "  uint tempResult = 0u;\n";

	if (OPTYPE_BROADCAST == caseDef.opType)
	{
		bdy	<< "  tempResult = 0x3u;\n";
		for (int i = 0; i < (int)subgroups::maxSupportedSubgroupSize(); i++)
		{
			bdy << "  {\n"
			<< "    const uint id = "<< i << "u;\n"
			<< "    " << subgroups::getFormatNameForGLSL(caseDef.format)
			<< " op = subgroupBroadcast(data1[gl_SubgroupInvocationID], id);\n"
			<< "    if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "    {\n"
			<< "      if (op != data1[id])\n"
			<< "      {\n"
			<< "        tempResult = 0u;\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
		}
	}
	else
	{
		bdy	<< "  uint firstActive = 0u;\n"
			<< "  for (uint i = 0u; i < gl_SubgroupSize; i++)\n"
			<< "  {\n"
			<< "    if (subgroupBallotBitExtract(mask, i))\n"
			<< "    {\n"
			<< "      firstActive = i;\n"
			<< "      break;\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempResult |= (subgroupBroadcastFirst(data1[gl_SubgroupInvocationID]) == data1[firstActive]) ? 0x1u : 0u;\n"
			<< "  // make the firstActive invocation inactive now\n"
			<< "  if (firstActive == gl_SubgroupInvocationID)\n"
			<< "  {\n"
			<< "    for (uint i = 0u; i < gl_SubgroupSize; i++)\n"
			<< "    {\n"
			<< "      if (subgroupBallotBitExtract(mask, i))\n"
			<< "      {\n"
			<< "        firstActive = i;\n"
			<< "        break;\n"
			<< "      }\n"
			<< "    }\n"
			<< "    tempResult |= (subgroupBroadcastFirst(data1[gl_SubgroupInvocationID]) == data1[firstActive]) ? 0x2u : 0u;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    // the firstActive invocation didn't partake in the second result so set it to true\n"
			<< "    tempResult |= 0x2u;\n"
			<< "  }\n";
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
		vertex << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
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

		geometry << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
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

		controlSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
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
		evaluationSource << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(binding = 0, std140) uniform Buffer0\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[" <<subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
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

		src << "${VERSION_DECL}\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
			<< "layout(binding = 0, std430) buffer Buffer0\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(binding = 1, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1[];\n"
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
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(binding = 0, std430) buffer Buffer0\n"
			"{\n"
			"  uint result[];\n"
			"} b0;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
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
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices=1) out;\n"
			"layout(binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"} b1;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
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
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines) in;\n"
			"layout(binding = 2, std430) buffer Buffer2\n"
			"{\n"
			"  uint result[];\n"
			"} b2;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			+ bdyStr +
			"  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			//version string added by addGeometryShadersFromTemplate
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(binding = 3, std430) buffer Buffer3\n"
			"{\n"
			"  uint result[];\n"
			"} b3;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
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
			"${VERSION_DECL}\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"precision highp int;\n"
			"precision highp float;\n"
			"layout(location = 0) out uint result;\n"
			"layout(binding = 4, std430) readonly buffer Buffer4\n"
			"{\n"
			"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data1[];\n"
			"};\n"
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
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
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
	inputData[0].format = caseDef.format;
	inputData[0].layout = subgroups::SSBOData::LayoutStd140;
	inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

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
		inputData[0].format = caseDef.format;
		inputData[0].layout = subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
		inputData[0].binding = 1u;

		return subgroups::makeComputeTest(context, FORMAT_R32_UINT, inputData, 1, checkComputeStages);
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

deqp::TestCaseGroup* createSubgroupsBallotBroadcastTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

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
			const std::string op = de::toLower(getOpTypeName(opTypeIndex));
			const std::string name = op + "_" + subgroups::getFormatNameForGLSL(format);

			{
				CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS, format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(), name + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "ballot_broadcast", "Subgroup ballot broadcast category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	return group.release();
}

} // subgroups
} // glc
