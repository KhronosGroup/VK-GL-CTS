/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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

#include "vktSubgroupsBallotOtherTests.hpp"
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
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0xf != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkFragment(std::vector<const void*> datas,
						  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = data[x * height + y];

			if (0xf != val)
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 nX = 0; nX < numWorkgroups[0]; ++nX)
	{
		for (deUint32 nY = 0; nY < numWorkgroups[1]; ++nY)
		{
			for (deUint32 nZ = 0; nZ < numWorkgroups[2]; ++nZ)
			{
				for (deUint32 lX = 0; lX < localSize[0]; ++lX)
				{
					for (deUint32 lY = 0; lY < localSize[1]; ++lY)
					{
						for (deUint32 lZ = 0; lZ < localSize[2];
								++lZ)
						{
							const deUint32 globalInvocationX =
								nX * localSize[0] + lX;
							const deUint32 globalInvocationY =
								nY * localSize[1] + lY;
							const deUint32 globalInvocationZ =
								nZ * localSize[2] + lZ;

							const deUint32 globalSizeX =
								numWorkgroups[0] * localSize[0];
							const deUint32 globalSizeY =
								numWorkgroups[1] * localSize[1];

							const deUint32 offset =
								globalSizeX *
								((globalSizeY *
								  globalInvocationZ) +
								 globalInvocationY) +
								globalInvocationX;

							if (0xf != data[offset])
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
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
	int opType;
	VkShaderStageFlags shaderStage;
};

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
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

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
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
				<< glu::ComputeSource(src.str());
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		std::ostringstream frag;

		frag << "#version 450\n"
			 << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			 << "layout(location = 0) out uint result;\n"
			 << "void main (void)\n"
			 << "{\n"
			 << bdy.str()
			 << "  result = tempResult;\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str());
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_VertexIndex] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(src.str());
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveIDIn] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("geom")
				<< glu::GeometrySource(src.str());
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource("#version 450\nlayout(isolines) in;\nvoid main (void) {}\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(src.str());
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource("#version 450\nlayout(vertices=1) out;\nvoid main (void) { for(uint i = 0; i < 4; i++) { gl_TessLevelOuter[i] = 1.0f; } }\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(src.str());
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if ((VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage) &&
			(VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage))
	{
		if (!subgroups::isVertexSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		}
	}

	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		return subgroups::makeFragmentTest(context, VK_FORMAT_R32_UINT,
										   DE_NULL, 0, checkFragment);
	}
	else if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT,
										  DE_NULL, 0, checkCompute);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		return subgroups::makeVertexTest(context, VK_FORMAT_R32_UINT,
										 DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		return subgroups::makeGeometryTest(context, VK_FORMAT_R32_UINT,
										   DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		return subgroups::makeTessellationControlTest(context, VK_FORMAT_R32_UINT,
				DE_NULL, 0, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		return subgroups::makeTessellationEvaluationTest(context, VK_FORMAT_R32_UINT,
				DE_NULL, 0, checkVertexPipelineStages);
	}
	return tcu::TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotOtherTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "ballot_other", "Subgroup ballot other category tests"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const VkShaderStageFlags stage = stages[stageIndex];

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			CaseDefinition caseDef = {opTypeIndex, stage};

			std::ostringstream name;

			std::string op = getOpTypeName(opTypeIndex);

			name << de::toLower(op) << "_" << getShaderStageName(stage);

			addFunctionCaseWithPrograms(group.get(), name.str(),
										"", initPrograms, test, caseDef);
		}
	}

	return group.release();
}

} // subgroups
} // vkt
