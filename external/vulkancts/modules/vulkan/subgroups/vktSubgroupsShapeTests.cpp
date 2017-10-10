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

#include "vktSubgroupsShapeTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
static bool checkFragment(std::vector<const void*> datas,
						  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* const resultData = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = resultData[(x * height + y)];

			if (0x1 != val)
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* const resultData = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = resultData[x];

		if (0x1 != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

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

							if (0x1 != data[offset])
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
		case OPTYPE_CLUSTERED:
			return "clustered";
		case OPTYPE_QUAD:
			return "quad";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	bool				noSSBO;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::string extension = (OPTYPE_CLUSTERED == caseDef.opType) ?
							"#extension GL_KHR_shader_subgroup_clustered: enable\n" :
							"#extension GL_KHR_shader_subgroup_quad: enable\n";

	extension += "#extension GL_KHR_shader_subgroup_ballot: enable\n";

	std::ostringstream bdy;

	bdy << "  uint tempResult = 0x1;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << ")\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    const uint modID = gl_SubgroupInvocationID % 32;\n"
				<< "    switch (gl_SubgroupInvocationID / 32)\n"
				<< "    {\n"
				<< "    case 0: contribution.x = 1 << modID; break;\n"
				<< "    case 1: contribution.y = 1 << modID; break;\n"
				<< "    case 2: contribution.z = 1 << modID; break;\n"
				<< "    case 3: contribution.w = 1 << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << ");\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << ");\n"
				<< "    for (uint i = 0; i < " << i << "; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  {\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3)\n"
			<< "  };\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3;\n"
			<< "  for (uint i = 0; i < 4; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream	src;
		std::ostringstream	fragmentSrc;

		src << "#version 450\n"
			<< extension
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float result;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result = float(tempResult);\n"
			<< "  gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in float result;\n"
			<< "layout(location = 0) out uint out_color;\n"
			<< "void main()\n"
			<<"{\n"
			<< "	out_color = uint(result);\n"
			<< "}\n";

		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
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

	bdy << "  uint tempResult = 0x1;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
		{
			bdy << "  if (gl_SubgroupSize >= " << i << ")\n"
				<< "  {\n"
				<< "    uvec4 contribution = uvec4(0);\n"
				<< "    const uint modID = gl_SubgroupInvocationID % 32;\n"
				<< "    switch (gl_SubgroupInvocationID / 32)\n"
				<< "    {\n"
				<< "    case 0: contribution.x = 1 << modID; break;\n"
				<< "    case 1: contribution.y = 1 << modID; break;\n"
				<< "    case 2: contribution.z = 1 << modID; break;\n"
				<< "    case 3: contribution.w = 1 << modID; break;\n"
				<< "    }\n"
				<< "    uvec4 result = subgroupClusteredOr(contribution, " << i << ");\n"
				<< "    uint rootID = gl_SubgroupInvocationID & ~(" << i - 1 << ");\n"
				<< "    for (uint i = 0; i < " << i << "; i++)\n"
				<< "    {\n"
				<< "      uint nextID = rootID + i;\n"
				<< "      if (subgroupBallotBitExtract(mask, nextID) ^^ subgroupBallotBitExtract(result, nextID))\n"
				<< "      {\n"
				<< "        tempResult = 0;\n"
				<< "      }\n"
				<< "    }\n"
				<< "  }\n";
		}
	}
	else
	{
		bdy << "  uint cluster[4] =\n"
			<< "  {\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 0),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 1),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 2),\n"
			<< "    subgroupQuadBroadcast(gl_SubgroupInvocationID, 3)\n"
			<< "  };\n"
			<< "  uint rootID = gl_SubgroupInvocationID & ~0x3;\n"
			<< "  for (uint i = 0; i < 4; i++)\n"
			<< "  {\n"
			<< "    uint nextID = rootID + i;\n"
			<< "    if (subgroupBallotBitExtract(mask, nextID) && (cluster[i] != nextID))\n"
			<< "    {\n"
			<< "      tempResult = mask.x;\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< extension
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
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream frag;

		frag << "#version 450\n"
			 << extension
			 << "layout(location = 0) out uint result;\n"
			 << "void main (void)\n"
			 << "{\n"
			 << bdy.str()
			 << "  result = tempResult;\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< extension
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
				<< glu::VertexSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		std::ostringstream src;

		src << "#version 450\n"
			<< extension
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
				<< glu::GeometrySource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource("#version 450\nlayout(isolines) in;\nvoid main (void) {}\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< extension
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID] = 1;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage)) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource("#version 450\nlayout(vertices=1) out;\nvoid main (void) { for(uint i = 0; i < 4; i++) { gl_TessLevelOuter[i] = 1.0f; } }\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< extension
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = 1;\n"
			<< "}\n";

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(src.str()) << vk::ShaderBuildOptions(vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
				   "Subgroup feature " +
				   subgroups::getShaderStageName(VK_SUBGROUP_FEATURE_BASIC_BIT) +
				   " is a required capability!");
	}

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (OPTYPE_CLUSTERED == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_CLUSTERED_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that clustered operations are supported!");
		}
	}

	if (OPTYPE_QUAD == caseDef.opType)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_QUAD_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup shape tests require that quad operations are supported!");
		}
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
			return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, checkVertexPipelineStages);
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
	else
	{
		TCU_THROW(InternalError, "Unhandled shader stage");
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsShapeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "shape", "Subgroup shape category tests"));

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
			CaseDefinition caseDef = {opTypeIndex, stage, false};

			std::string op = getOpTypeName(opTypeIndex);

			addFunctionCaseWithPrograms(group.get(),
										de::toLower(op) + "_" + getShaderStageName(stage), "",
										initPrograms, test, caseDef);

			if (VK_SHADER_STAGE_VERTEX_BIT & stage )
			{
				caseDef.noSSBO = true;
				addFunctionCaseWithPrograms(group.get(), de::toLower(op) + "_" + getShaderStageName(stage) + "_framebuffer", "",
											initFrameBufferPrograms, test, caseDef);
			}
		}
	}

	return group.release();
}

} // subgroups
} // vkt
